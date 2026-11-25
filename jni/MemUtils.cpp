#include "MemUtils.h"
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <cinttypes>
#include <vector>
#include <algorithm>
#include <android/log.h>
#include "Logging.h"

#ifndef MREMAP_MAYMOVE
#define MREMAP_MAYMOVE 1
#endif
#ifndef MREMAP_FIXED
#define MREMAP_FIXED 2
#endif

extern "C" void __clear_cache(void* beg, void* end);

uintptr_t g_libAddress = 0;
size_t g_libSize = 0;

struct MapRange {
    uintptr_t start = 0;
    uintptr_t end = 0;
    uintptr_t fileOffset = 0;
    bool readable = false;
    bool executable = false;
};

static std::vector<MapRange> g_libRanges;

void ResetLibraryBounds()
{
    g_libAddress = 0;
    g_libSize = 0;
    g_libRanges.clear();
}

bool ResolveLibraryBounds(const char* libName)
{
    if (g_libAddress != 0 && g_libSize != 0 && !g_libRanges.empty())
        return true;

    FILE* fp = fopen("/proc/self/maps", "rt");
    if (fp == NULL) {
        perror("fopen");
        return false;
    }

    uintptr_t firstStart = 0;
    uintptr_t lastEnd = 0;
    char line[1024];
    std::vector<MapRange> ranges;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, libName) == NULL)
            continue;

        uintptr_t start = 0, end = 0, fileOff = 0;
        char perms[8] = {};
        // address-address perms offset ...
        if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %7s %" SCNxPTR, &start, &end, perms, &fileOff) < 3)
            continue;

        if (firstStart == 0)
            firstStart = start;
        lastEnd = end;

        MapRange r;
        r.start = start;
        r.end = end;
        r.fileOffset = fileOff;
        r.readable = (perms[0] == 'r');
        r.executable = (perms[2] == 'x');
        ranges.push_back(r);
    }

    fclose(fp);

    if (firstStart == 0 || lastEnd <= firstStart || ranges.empty())
        return false;

    g_libAddress = firstStart;
    g_libSize = lastEnd - firstStart;
    g_libRanges = std::move(ranges);
    return true;
}

uintptr_t GetActualOffset(uintptr_t offset)
{
    ResolveLibraryBounds("libSrc.so");
    return g_libAddress + offset;
}

static void BarrierICache(void* addr, size_t size)
{
    __clear_cache(reinterpret_cast<char*>(addr), reinterpret_cast<char*>(addr) + size);
#if defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("dmb ish" ::: "memory");
    __asm__ __volatile__("isb" ::: "memory");
#endif
}

/**
 * Replace one page with an anonymous RX copy (patched).
 * Required on Samsung/Android where APK-backed RX pages reject mprotect(WRITE)
 * or reject restoring PROT_EXEC after a temporary RW change.
 */
static bool ReplaceBytesViaRemap(uintptr_t addr, const void* data, size_t size)
{
    size_t pagesize = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    uintptr_t pageStart = addr & ~(pagesize - 1);
    size_t pageOff = static_cast<size_t>(addr - pageStart);
    if (pageOff + size > pagesize) {
        LOGE("ReplaceBytesViaRemap: patch crosses page boundary");
        return false;
    }

    void* copy = mmap(nullptr, pagesize, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (copy == MAP_FAILED) {
        LOGE("ReplaceBytesViaRemap: mmap copy failed errno=%d", errno);
        return false;
    }

    // Execute-only (--x) pages cannot be read; temporarily allow READ for the copy.
    if (mprotect(reinterpret_cast<void*>(pageStart), pagesize, PROT_READ | PROT_EXEC) != 0) {
        if (mprotect(reinterpret_cast<void*>(pageStart), pagesize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
            // Still try the copy — page may already be readable (r-xp).
            LOGW("ReplaceBytesViaRemap: source mprotect READ failed errno=%d (trying copy anyway)", errno);
        }
    }

    memcpy(copy, reinterpret_cast<void*>(pageStart), pagesize);
    memcpy(reinterpret_cast<char*>(copy) + pageOff, data, size);

    if (mprotect(copy, pagesize, PROT_READ | PROT_EXEC) != 0) {
        if (mprotect(copy, pagesize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
            LOGE("ReplaceBytesViaRemap: mprotect RX/RWX failed errno=%d", errno);
            munmap(copy, pagesize);
            return false;
        }
    }

    void* remapped = mremap(copy, pagesize, pagesize, MREMAP_MAYMOVE | MREMAP_FIXED, reinterpret_cast<void*>(pageStart));
    if (remapped == MAP_FAILED) {
        LOGE("ReplaceBytesViaRemap: mremap FIXED failed errno=%d", errno);
        munmap(copy, pagesize);
        return false;
    }

    BarrierICache(reinterpret_cast<void*>(addr), size);
    LOGI("ReplaceBytesViaRemap: OK page @%p", reinterpret_cast<void*>(pageStart));
    return true;
}

bool ReplaceBytes(uintptr_t offset, const void* data, size_t size)
{
    uintptr_t addr = GetActualOffset(offset);

    if (ReplaceBytesViaRemap(addr, data, size)) {
        return true;
    }

    LOGE("ReplaceBytes: all strategies failed errno=%d", errno);
    return false;
}

static bool MatchAt(uintptr_t addr, const uint8_t* pattern, const char* mask, size_t patternLength)
{
    for (size_t j = 0; j < patternLength; ++j) {
        if (mask[j] == 'x' &&
            *(reinterpret_cast<const uint8_t*>(addr + j)) != pattern[j]) {
            return false;
        }
    }
    return true;
}

/**
 * Scan one mapped range for the pattern. Returns RVA from g_libAddress, or PATTERN_NOT_FOUND.
 */
static size_t ScanRange(const MapRange& range, const uint8_t* pattern, const char* mask, size_t patternLength)
{
    if (range.end <= range.start)
        return PATTERN_NOT_FOUND;

    size_t size = range.end - range.start;
    if (patternLength > size)
        return PATTERN_NOT_FOUND;

    uintptr_t start = range.start;
    for (size_t i = 0; i <= size - patternLength; ++i) {
        if (MatchAt(start + i, pattern, mask, patternLength)) {
            return static_cast<size_t>((start + i) - g_libAddress);
        }
    }
    return PATTERN_NOT_FOUND;
}

/**
 * Prefer patching an executable mapping. If the hit was in a readable-only alias
 * (r-- vs --x split / XOM), translate via file offset to the executable VA.
 */
static size_t PreferExecutableRva(size_t hitRva)
{
    uintptr_t hitVa = g_libAddress + hitRva;
    const MapRange* hitRange = nullptr;
    for (const MapRange& range : g_libRanges) {
        if (hitVa >= range.start && hitVa < range.end) {
            hitRange = &range;
            break;
        }
    }
    if (!hitRange)
        return hitRva;
    if (hitRange->executable)
        return hitRva;

    uintptr_t fileOff = hitRange->fileOffset + (hitVa - hitRange->start);
    for (const MapRange& range : g_libRanges) {
        if (!range.executable)
            continue;
        if (range.end <= range.start)
            continue;
        uintptr_t rel = fileOff - range.fileOffset;
        if (fileOff >= range.fileOffset && rel < (range.end - range.start)) {
            uintptr_t execVa = range.start + rel;
            size_t execRva = static_cast<size_t>(execVa - g_libAddress);
            LOGI("FindPattern: remapped r-- hit file+0x%zx -> exec RVA 0x%zx", (size_t)fileOff, execRva);
            return execRva;
        }
    }
    return hitRva;
}

std::vector<size_t> FindAllInstancesOf(const uint8_t* pattern, const char* mask)
{
    std::vector<size_t> matches;
    ResetLibraryBounds();
    if (!ResolveLibraryBounds("libSrc.so")) {
        LOGE("FindAllInstancesOf: libSrc.so not mapped");
        return matches;
    }

    const size_t patternLength = strlen(mask);
    if (patternLength == 0)
        return matches;

    for (MapRange& range : g_libRanges) {
        if (range.end <= range.start)
            continue;
        const size_t size = range.end - range.start;
        if (patternLength > size)
            continue;
        if (!range.readable) {
            if (!range.executable)
                continue;
            if (mprotect(reinterpret_cast<void*>(range.start), size, PROT_READ | PROT_EXEC) != 0) {
                LOGW("FindAllInstancesOf: mprotect READ|EXEC failed @%p errno=%d", reinterpret_cast<void*>(range.start), errno);
                continue;
            }
            range.readable = true;
        }
        for (size_t i = 0; i <= size - patternLength; ++i) {
            if (MatchAt(range.start + i, pattern, mask, patternLength)) {
                matches.push_back(PreferExecutableRva(static_cast<size_t>(range.start + i - g_libAddress)));
            }
        }
    }
    std::sort(matches.begin(), matches.end());
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
    return matches;
}

size_t FindPattern(const uint8_t* pattern, const char* mask)
{
    ResetLibraryBounds();
    if (!ResolveLibraryBounds("libSrc.so")) {
        LOGE("FindPattern: libSrc.so not mapped");
        return PATTERN_NOT_FOUND;
    }

    size_t patternLength = strlen(mask);
    if (patternLength == 0)
        return PATTERN_NOT_FOUND;

    int readableCount = 0;
    int execCount = 0;
    for (const MapRange& range : g_libRanges) {
        if (range.readable)
            readableCount++;
        if (range.executable)
            execCount++;
    }

    // 1) Classic r-xp (or any readable+executable) ranges first.
    for (const MapRange& range : g_libRanges) {
        if (!range.readable || !range.executable)
            continue;
        size_t hit = ScanRange(range, pattern, mask, patternLength);
        if (hit != PATTERN_NOT_FOUND)
            return hit;
    }

    // 2) Readable-only aliases (common with execute-only memory: r-- + --x split).
    for (const MapRange& range : g_libRanges) {
        if (!range.readable || range.executable)
            continue;
        size_t hit = ScanRange(range, pattern, mask, patternLength);
        if (hit != PATTERN_NOT_FOUND)
            return PreferExecutableRva(hit);
    }

    // 3) Execute-only (--x): temporarily add PROT_READ so we can scan, then patch there.
    for (MapRange& range : g_libRanges) {
        if (!range.executable || range.readable)
            continue;
        size_t size = range.end - range.start;
        if (mprotect(reinterpret_cast<void*>(range.start), size, PROT_READ | PROT_EXEC) != 0) {
            LOGW("FindPattern: mprotect READ|EXEC failed @%p errno=%d", reinterpret_cast<void*>(range.start), errno);
            continue;
        }
        range.readable = true;
        size_t hit = ScanRange(range, pattern, mask, patternLength);
        if (hit != PATTERN_NOT_FOUND)
            return hit;
    }

    LOGE("FindPattern: no match (ranges=%zu readable=%d exec=%d)", g_libRanges.size(), readableCount, execCount);
    return PATTERN_NOT_FOUND;
}
