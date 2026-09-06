#include "MemUtils.h"
#include "Logging.h"
#include <android/log.h>
#include <cerrno>
#include <cstring>

static bool g_accountBypassDone = false;

bool AccountBindingBypass()
{
	if (g_accountBypassDone) {
		return true;
	}

	// Always re-read maps (hot-update remaps libSrc; XOM layouts need fresh ranges).
	ResetLibraryBounds();

	const char* mask = "xxxx";
#ifdef __arm__
	uint8_t pattern[] = { 0x47, 0xF2, 0x62, 0x50 };
	uintptr_t foundPattern = FindPattern(pattern, mask);
	if (foundPattern == PATTERN_NOT_FOUND) {
		__android_log_print(ANDROID_LOG_ERROR, LIB_TAG, "AccountBindingBypass: 32-bit pattern not found");
		return false;
	}
	uint32_t replaceValue = 0x0000F240;
	if (!ReplaceBytes(foundPattern, &replaceValue, 4)) {
		__android_log_print(ANDROID_LOG_ERROR, LIB_TAG, "AccountBindingBypass: 32-bit ReplaceBytes failed errno=%d", errno);
		return false;
	}
	g_accountBypassDone = true;
	__android_log_print(ANDROID_LOG_INFO, LIB_TAG, "AccountBindingBypass: 32-bit patched @0x%zx", (size_t)foundPattern);
	return true;
#elif defined __aarch64__
	uint8_t pattern[] = { 0x4A, 0xAC, 0x8E, 0x52 };
	uintptr_t foundPattern = FindPattern(pattern, mask);
	if (foundPattern == PATTERN_NOT_FOUND) {
		__android_log_print(ANDROID_LOG_ERROR, LIB_TAG, "AccountBindingBypass: 64-bit pattern not found");
		return false;
	}
	uint32_t replaceValue = 0x5280000A;
	if (!ReplaceBytes(foundPattern, &replaceValue, 4)) {
		__android_log_print(ANDROID_LOG_ERROR, LIB_TAG, "AccountBindingBypass: 64-bit ReplaceBytes failed errno=%d", errno);
		return false;
	}
	g_accountBypassDone = true;
	__android_log_print(ANDROID_LOG_INFO, LIB_TAG, "AccountBindingBypass: 64-bit patched @0x%zx", (size_t)foundPattern);
	return true;
#else
	return false;
#endif
}

void HooksInit()
{
	AccountBindingBypass();
}
