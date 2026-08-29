#include "MemUtils.h"
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>
#include <cstdlib>
#include <cinttypes>
#include <vector>

extern "C" void __clear_cache(void* beg, void* end);

uintptr_t g_libAddress = 0;
size_t g_libSize = 0;

struct MapRange {
    uintptr_t start = 0;
    uintptr_t end = 0;
    bool readable = false;
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
        // e.g. 7a00000000-7a01000000 r-xp 00000000 ...
        // PRIxPTR-width scan: %lx is wrong for 32-bit uintptr_t
        if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %7s", &start, &end, perms) < 2)
            continue;

        if (firstStart == 0)
            firstStart = start;
        lastEnd = end;

        MapRange r;
        r.start = start;
        r.end = end;
        r.readable = (perms[0] == 'r');
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

bool ReplaceBytes(uintptr_t offset, const void* data, size_t size)
{
    uintptr_t addr = GetActualOffset(offset);
    size_t pagesize = sysconf(_SC_PAGESIZE);

    uintptr_t pageStart = addr & ~(pagesize - 1);
    uintptr_t pageEnd = (addr + size + pagesize - 1) & ~(pagesize - 1);
    size_t totalSize = pageEnd - pageStart;

    // Android often rejects RWX in one shot — use RW, write, then RX.
    if (mprotect((void*)pageStart, totalSize, PROT_READ | PROT_WRITE) != 0) {
        return false;
    }

    memcpy((void*)addr, data, size);
    __clear_cache((char*)addr, (char*)addr + size);

#if defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("dmb ish" ::: "memory");
    __asm__ __volatile__("isb" ::: "memory");
#endif

    if (mprotect((void*)pageStart, totalSize, PROT_READ | PROT_EXEC) != 0) {
        return false;
    }

#if defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("isb" ::: "memory");
#endif

    return true;
}

// Modified to support resuming a scan from a given RVA offset.
// Existing callers that don't pass startOffset behave exactly as before.
size_t FindPattern(const uint8_t* pattern, const char* mask, size_t startOffset)
{
    if (!ResolveLibraryBounds("libSrc.so"))
        return PATTERN_NOT_FOUND;

    size_t patternLength = strlen(mask);
    if (patternLength == 0)
        return PATTERN_NOT_FOUND;

    // Only scan readable mappings — crossing ---p gaps SIGSEGVs the init thread.
    for (const MapRange& range : g_libRanges) {
        if (!range.readable)
            continue;
        if (range.end <= range.start)
            continue;

        size_t size = range.end - range.start;
        if (patternLength > size)
            continue;

        uintptr_t start = range.start;

        // Skip this whole range if it lies entirely before startOffset.
        uintptr_t rangeStartRVA = start - g_libAddress;
        size_t scanBeginIndex = 0;
        if (rangeStartRVA < startOffset) {
            size_t skip = startOffset - rangeStartRVA;
            if (skip >= size)
                continue; // whole range already scanned
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

// Repeatedly calls FindPattern, resuming just past each hit,
// until no more matches are found. Returns every RVA where the pattern occurred.
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

        // Advance past this match to avoid re-finding the same spot.
        // Use +1 instead of +patternLength if you want overlapping matches too.
        searchFrom = found + patternLength;
    }

    return results;
}