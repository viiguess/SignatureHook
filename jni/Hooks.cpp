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

void SetMaxBoardRenderScale()
{
#ifdef __arm__
	const char* mask = "xxxxxxxx";
	uint8_t pattern[] = {0xB0, 0xEE, 0x41, 0x5A, 0x8A, 0xED, 0x11, 0x5A};
	uintptr_t foundPattern = FindPattern(pattern, mask);
	if (foundPattern == PATTERN_NOT_FOUND) {
		__android_log_print(ANDROID_LOG_ERROR, LIB_TAG, "SetMaxBoardRenderScale: 32-bit pattern not found");
		return;
	}
	uint32_t replaceValue = 0x5A00EEB7;
	if (!ReplaceBytes(foundPattern, &replaceValue, 4)) {
		__android_log_print(ANDROID_LOG_ERROR, LIB_TAG, "SetMaxBoardRenderScale: 32-bit ReplaceBytes failed errno=%d", errno);
	} else {
		__android_log_print(ANDROID_LOG_INFO, LIB_TAG, "SetMaxBoardRenderScale: 32-bit patched @0x%zx", (size_t)foundPattern);
	}
#elif defined __aarch64__
	const char* mask = "xxxxxxxxxxxxxxxx";
	uint8_t pattern[] = {0x03, 0x04, 0x0C, 0x5E, 0x00, 0x78, 0x23, 0x1E, 0x03, 0x10, 0x2E, 0x1E, 0x00, 0x68, 0x23, 0x1E};
	uint8_t replacement[] = {0x1F, 0x20, 0x03, 0xD5, 0x1F, 0x20, 0x03, 0xD5, 0x00, 0x10, 0x2E, 0x1E, 0x1F, 0x20, 0x03, 0xD5};

	std::vector<size_t> patterns = FindAllInstancesOf(pattern, mask);

	for(size_t i = 0; i < patterns.size(); i++)
	{
		if (!ReplaceBytes(patterns[i], replacement, sizeof(replacement)))
		{
			__android_log_print(ANDROID_LOG_ERROR, LIB_TAG, "SetMaxBoardRenderScale: 64-bit ReplaceBytes failed @0x%zx errno=%d", patterns[i], errno);
		}
		else
		{
			__android_log_print(ANDROID_LOG_INFO, LIB_TAG, "SetMaxBoardRenderScale: 64-bit patched @0x%zx", patterns[i]);
		}
	}
#endif
}

void HooksInit()
{
	AccountBindingBypass();
	SetMaxBoardRenderScale();
}
