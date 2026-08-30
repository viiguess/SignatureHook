#include "MemUtils.h"
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <cinttypes>
#include <vector>
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
    bool readable = false;
    bool executable = false;
};

static std::vector<MapRange> g_libRanges;

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

        uintptr_t start = 0, end = 0;
        char perms[8] = {};
        if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %7s", &start, &end, perms) < 2)
            continue;

        if (firstStart == 0)
            firstStart = start;
        lastEnd = end;

        MapRange r;
        r.start = start;
        r.end = end;
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
        __android_log_print(ANDROID_LOG_ERROR, LIB_TAG,
                            "ReplaceBytesViaRemap: patch crosses page boundary");
        return false;
    }

    void* copy = mmap(nullptr, pagesize, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (copy == MAP_FAILED) {
        __android_log_print(ANDROID_LOG_ERROR, LIB_TAG,
                            "ReplaceBytesViaRemap: mmap copy failed errno=%d", errno);
        return false;
    }

    memcpy(copy, reinterpret_cast<void*>(pageStart), pagesize);
    memcpy(reinterpret_cast<char*>(copy) + pageOff, data, size);

    if (mprotect(copy, pagesize, PROT_READ | PROT_EXEC) != 0) {
        if (mprotect(copy, pagesize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
            __android_log_print(ANDROID_LOG_ERROR, LIB_TAG,
                                "ReplaceBytesViaRemap: mprotect RX/RWX failed errno=%d", errno);
            munmap(copy, pagesize);
            return false;
        }
    }

    void* remapped = mremap(copy, pagesize, pagesize,
                            MREMAP_MAYMOVE | MREMAP_FIXED,
                            reinterpret_cast<void*>(pageStart));
    if (remapped == MAP_FAILED) {
        __android_log_print(ANDROID_LOG_ERROR, LIB_TAG,
                            "ReplaceBytesViaRemap: mremap FIXED failed errno=%d", errno);
        munmap(copy, pagesize);
        return false;
    }

    BarrierICache(reinterpret_cast<void*>(addr), size);
    __android_log_print(ANDROID_LOG_INFO, LIB_TAG,
                        "ReplaceBytesViaRemap: OK page @%p",
                        reinterpret_cast<void*>(pageStart));
    return true;
}

bool ReplaceBytes(uintptr_t offset, const void* data, size_t size)
{
    uintptr_t addr = GetActualOffset(offset);

    if (ReplaceBytesViaRemap(addr, data, size)) {
        return true;
    }

    __android_log_print(ANDROID_LOG_ERROR, LIB_TAG,
                        "ReplaceBytes: all strategies failed errno=%d", errno);
    return false;
}

size_t FindPattern(const uint8_t* pattern, const char* mask, size_t startOffset)
{
    if (!ResolveLibraryBounds("libSrc.so"))
        return PATTERN_NOT_FOUND;

    size_t patternLength = strlen(mask);
    if (patternLength == 0)
        return PATTERN_NOT_FOUND;

    for (const MapRange& range : g_libRanges) {
        if (!range.readable || !range.executable)
            continue;
        if (range.end <= range.start)
            continue;

        size_t size = range.end - range.start;
        if (patternLength > size)
            continue;

        uintptr_t start = range.start;

        uintptr_t rangeStartRVA = start - g_libAddress;
        size_t scanBeginIndex = 0;
        if (rangeStartRVA < startOffset) {
            size_t skip = startOffset - rangeStartRVA;
            if (skip >= size)
                continue;
            scanBeginIndex = skip;
        }

        for (size_t i = scanBeginIndex; i <= size - patternLength; ++i) {
            bool found = true;
            for (size_t j = 0; j < patternLength; ++j) {
                if (mask[j] == 'x' &&
                    *(reinterpret_cast<const uint8_t*>(start + i + j)) != pattern[j]) {
                    found = false;
                    break;
                }
            }
            if (found) {
                size_t rva = static_cast<size_t>((start + i) - g_libAddress);
                if (rva >= startOffset)
                    return rva;
            }
        }
    }

    return PATTERN_NOT_FOUND;
}

std::vector<size_t> FindAllInstancesOf(const uint8_t* pattern, const char* mask)
{
    std::vector<size_t> results;

    size_t patternLength = strlen(mask);
    if (patternLength == 0)
        return results;

    size_t searchFrom = 0;
    while (true) {
        size_t found = FindPattern(pattern, mask, searchFrom);
        if (found == PATTERN_NOT_FOUND)
            break;

        results.push_back(found);
        searchFrom = found + patternLength;
    }

    return results;
}
