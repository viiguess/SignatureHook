#include "MemUtils.h"
#include "Logging.h"
#include <android/log.h>
#include <cerrno>
#include <cstring>

static bool g_accountBypassDone = false;
static bool g_boardRenderScaleDone = false;

bool AccountBindingBypass()
{
	if (g_accountBypassDone) {
		return true;
	}

	const char* mask = "xxxx";
#ifdef __arm__
	uint8_t pattern[] = { 0x47, 0xF2, 0x62, 0x50 };
	uintptr_t foundPattern = FindPattern(pattern, mask);
	if (foundPattern == PATTERN_NOT_FOUND) {
		LOGE("AccountBindingBypass: 32-bit pattern not found");
		return false;
	}
	uint32_t replaceValue = 0x0000F240;
	if (!ReplaceBytes(foundPattern, &replaceValue, 4)) {
		LOGE("AccountBindingBypass: 32-bit ReplaceBytes failed errno=%d", errno);
		return false;
	}
	g_accountBypassDone = true;
	LOGI("AccountBindingBypass: 32-bit patched @0x%zx", (size_t)foundPattern);
	return true;
#elif defined __aarch64__
	uint8_t pattern[] = { 0x4A, 0xAC, 0x8E, 0x52 };
	uintptr_t foundPattern = FindPattern(pattern, mask);
	if (foundPattern == PATTERN_NOT_FOUND) {
		LOGE("AccountBindingBypass: 64-bit pattern not found");
		return false;
	}
	uint32_t replaceValue = 0x5280000A;
	if (!ReplaceBytes(foundPattern, &replaceValue, 4)) {
		LOGE("AccountBindingBypass: 64-bit ReplaceBytes failed errno=%d", errno);
		return false;
	}
	g_accountBypassDone = true;
	LOGI("AccountBindingBypass: 64-bit patched @0x%zx", (size_t)foundPattern);
	return true;
#else
	return false;
#endif
}

// Exactly the same as AltverZ and Reflourished Max Lawn Zoom setting
bool SetMaxBoardRenderScale()
{
	if (g_boardRenderScaleDone) {
		return true;
	}

#ifdef __arm__
	const char* mask = "xxxxxxxx";
	const uint8_t pattern[] = { 0xB0, 0xEE, 0x41, 0x5A, 0x8A, 0xED, 0x11, 0x5A };
	const size_t foundPattern = FindPattern(pattern, mask);
	if (foundPattern == PATTERN_NOT_FOUND) {
		LOGE("SetMaxBoardRenderScale: 32-bit pattern not found");
		return false;
	}
	const uint32_t replacement = 0x5A00EEB7;
	if (!ReplaceBytes(foundPattern, &replacement, sizeof(replacement))) {
		LOGE("SetMaxBoardRenderScale: 32-bit ReplaceBytes failed errno=%d", errno);
		return false;
	}
	g_boardRenderScaleDone = true;
	LOGI("SetMaxBoardRenderScale: 32-bit patched @0x%zx", foundPattern);
	return true;
#elif defined(__aarch64__)
	const char* mask = "xxxxxxxxxxxxxxxx";
	const uint8_t pattern[] = { 0x03, 0x04, 0x0C, 0x5E, 0x00, 0x78, 0x23, 0x1E, 0x03, 0x10, 0x2E, 0x1E, 0x00, 0x68, 0x23, 0x1E };
	const uint8_t replacement[] = { 0x1F, 0x20, 0x03, 0xD5, 0x1F, 0x20, 0x03, 0xD5, 0x00, 0x10, 0x2E, 0x1E, 0x1F, 0x20, 0x03, 0xD5 };
	const std::vector<size_t> patterns = FindAllInstancesOf(pattern, mask);
	if (patterns.empty()) {
		LOGE("SetMaxBoardRenderScale: 64-bit pattern not found");
		return false;
	}
	for (size_t offset : patterns) {
		if (!ReplaceBytes(offset, replacement, sizeof(replacement))) {
			LOGE("SetMaxBoardRenderScale: 64-bit ReplaceBytes failed @0x%zx errno=%d", offset, errno);
			g_boardRenderScaleDone = false;
		} else {
			LOGI("SetMaxBoardRenderScale: 64-bit patched @0x%zx", offset);
		}
	}
	g_boardRenderScaleDone = true;
#else
	return false;
#endif
}

void HooksInit()
{
	AccountBindingBypass();
	SetMaxBoardRenderScale();
}
