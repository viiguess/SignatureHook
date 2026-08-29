#include "MemUtils.h"
#include <cerrno>
#include <cstring>

void AccountBindingBypass()
{
	const char* mask = "xxxx";
#ifdef __arm__
	// MOVW R0, #0x7562 ("bu" from channel "bui") — must be 47 F2 62 50, not 47 F2 00 50
	uint8_t pattern[] = { 0x47, 0xF2, 0x62, 0x50 };
	uintptr_t foundPattern = FindPattern(pattern, mask);
	if (foundPattern == PATTERN_NOT_FOUND) {
		__android_log_print(ANDROID_LOG_ERROR, LIB_TAG, "AccountBindingBypass: 32-bit pattern not found");
		return;
	}
	uint32_t replaceValue = 0x0000F240; // MOVW R0, #0 → bytes 40 F2 00 00
	if (!ReplaceBytes(foundPattern, &replaceValue, 4)) {
		__android_log_print(ANDROID_LOG_ERROR, LIB_TAG, "AccountBindingBypass: 32-bit ReplaceBytes failed errno=%d", errno);
	} else {
		__android_log_print(ANDROID_LOG_INFO, LIB_TAG, "AccountBindingBypass: 32-bit patched @0x%zx", (size_t)foundPattern);
	}
#elif defined __aarch64__
	// MOV W10, #0x7562 ("bu" from channel "bui")
	uint8_t pattern[] = { 0x4A, 0xAC, 0x8E, 0x52 };
	uintptr_t foundPattern = FindPattern(pattern, mask);
	if (foundPattern == PATTERN_NOT_FOUND) {
		__android_log_print(ANDROID_LOG_ERROR, LIB_TAG, "AccountBindingBypass: 64-bit pattern not found");
		return;
	}
	uint32_t replaceValue = 0x5280000A; // MOV W10, #0 → bytes 0A 00 80 52
	if (!ReplaceBytes(foundPattern, &replaceValue, 4)) {
		__android_log_print(ANDROID_LOG_ERROR, LIB_TAG, "AccountBindingBypass: 64-bit ReplaceBytes failed errno=%d", errno);
	} else {
		__android_log_print(ANDROID_LOG_INFO, LIB_TAG, "AccountBindingBypass: 64-bit patched @0x%zx", (size_t)foundPattern);
	}
#endif
}

void HooksInit()
{
	AccountBindingBypass();
}
