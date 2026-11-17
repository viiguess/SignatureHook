#pragma once
#include <unistd.h>
#include <cstdlib>
#include <stdio.h>
#include <cstddef>
#include <cstdint>
#include <vector>

#ifdef __aarch64__
#define A64
#define ARMSWITCH(arm64Value, arm32Value) arm64Value
#else
#define ARMSWITCH(arm64Value, arm32Value) arm32Value
#endif

/** Returned by FindPattern when no match (0 is a valid file/RVA offset). */
inline constexpr size_t PATTERN_NOT_FOUND = static_cast<size_t>(-1);

extern uintptr_t g_libAddress;
/** Clear cached /proc/self/maps state so the next resolve re-reads mappings. */
void ResetLibraryBounds();
// Get base address of a library (.so) loaded in memory.
bool ResolveLibraryBounds(const char* libName);
// Get actual offset of address inside libSrc.so
uintptr_t GetActualOffset(uintptr_t offset);
// Modify bytes in libSrc.so
bool ReplaceBytes(uintptr_t offset, const void* data, size_t size);
// Find first pattern match; returns RVA from library base, or PATTERN_NOT_FOUND.
size_t FindPattern(const uint8_t* pattern, const char* mask);
// Find all matches as unique RVAs, translating readable aliases to executable mappings.
std::vector<size_t> FindAllInstancesOf(const uint8_t* pattern, const char* mask);
