#include "stdafx.h"
#include <MMSystem.h>

uintptr_t sub_490860_addr;
uintptr_t sub_49D910_addr;
TIMECAPS tc;
LARGE_INTEGER Frequency;
LARGE_INTEGER PreviousTime, CurrentTime, ElapsedMicroseconds;
int sleepTime;
int framerateFactor;

struct Variables
{
	uint32_t nWidth;
	uint32_t nHeight;
	float fAspectRatio;
	float fScaleValue;
	float f2DScaleValue;
	uint32_t* speedMultiplier;
	bool* isDemoMode;
} Variables;

void UpdateElapsedMicroseconds() {
	QueryPerformanceCounter(&CurrentTime);
	ElapsedMicroseconds.QuadPart = CurrentTime.QuadPart - PreviousTime.QuadPart;
	ElapsedMicroseconds.QuadPart *= 1000000;
	ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
}

int __cdecl sub_490860(int a1) {
	timeBeginPeriod(tc.wPeriodMin);

	if (PreviousTime.QuadPart == 0)
		QueryPerformanceCounter(&PreviousTime); // initialise

	UpdateElapsedMicroseconds();

	framerateFactor = ((int)ElapsedMicroseconds.QuadPart / 16667) + 1;
	// Demo mode needs 30fps maximum
	if (*Variables.isDemoMode && framerateFactor < 2)
		framerateFactor = 2;
		
	*Variables.speedMultiplier = std::clamp(framerateFactor, 1, 3);

	sleepTime = 0;
	// Loop until next frame due
	do {
		sleepTime = (16949 * framerateFactor - (uint32_t)ElapsedMicroseconds.QuadPart) / 1000; // calculate sleep time, 16949 µs = 59 fps (to limit frame drops)
		sleepTime = ((sleepTime / tc.wPeriodMin) * tc.wPeriodMin) - tc.wPeriodMin; // truncate to multiple of period
		if (sleepTime > 0)
			Sleep(sleepTime); // sleep to avoid wasted CPU
		UpdateElapsedMicroseconds();
	} while (ElapsedMicroseconds.QuadPart < 16667 * framerateFactor);

	QueryPerformanceCounter(&PreviousTime);
	timeEndPeriod(tc.wPeriodMin);
	return (int)(PreviousTime.QuadPart / 1000);
}

int sub_49D910() {
	auto _sub_49D910 = (int(*)()) sub_49D910_addr;

	/* FIX WIDESCREEN */
	// this code can't be in Init() because width/height are not set at first

	/* Set width and height */
	auto pattern = hook::pattern("8B 15 ? ? ? ? 89 4C 24 08 89 44 24 0C"); //4B5672
	Variables.nWidth = *(int*)pattern.get_first(2);
	Variables.nHeight = *((int*)pattern.get_first(2) + 4);
	Variables.fAspectRatio = float(Variables.nWidth) / float(Variables.nHeight);
	Variables.fScaleValue = 1.0f / Variables.fAspectRatio;
	Variables.f2DScaleValue = (4.0f / 3.0f) / Variables.fAspectRatio;

	/* Fix 3D stretch */
	pattern = hook::pattern("C7 40 44 00 00 40 3F"); //4CE80F
	struct Widescreen3DHook
	{
		void operator()(injector::reg_pack& regs)
		{
			float* ptrScaleValue = (float*)regs.eax + 0x44;
			*ptrScaleValue = Variables.fScaleValue;
		}
	}; injector::MakeInline<Widescreen3DHook>(pattern.get_first(0), pattern.get_first(6));


	return _sub_49D910();
}



DWORD WINAPI Init(LPVOID bDelay)
{
	/* INITIALISE */
	auto pattern = hook::pattern("03 D1 2B D7 85 D2 7E 09 52 E8 ? ? ? ?"); //4909B5

	if (pattern.count_hint(1).empty() && !bDelay)
	{
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)&Init, (LPVOID)true, 0, NULL);
		return 0;
	}

	if (bDelay)
		while (pattern.clear().count_hint(1).empty()) { Sleep(0); };

	CIniReader iniReader("ToyStory2Fix.ini");
	constexpr char* INI_KEY = "ToyStory2Fix";

	if (iniReader.ReadBoolean(INI_KEY, "FixFramerate", true)) {
		timeGetDevCaps(&tc, sizeof(tc));
		QueryPerformanceFrequency(&Frequency);

		pattern = hook::pattern("8B 0D ? ? ? ? 2B F1 3B"); //4011DF
		Variables.speedMultiplier = *(uint32_t**)pattern.get_first<uint32_t**>(2);
		pattern = hook::pattern("39 3D ? ? ? ? 75 27"); //403C3A
  		Variables.isDemoMode = *(bool**)pattern.get_first<bool*>(2);

		pattern = hook::pattern("C7 05 ? ? ? ? 00 00 00 00 E8 ? ? ? ? E8 ? ? ? ? 33"); //49BBD8
		sub_490860_addr = ((uintptr_t)pattern.get_first(15) + *pattern.get_first<uintptr_t>(11));
		injector::MakeCALL(pattern.get_first(10), sub_490860);
		pattern = hook::pattern("83 C4 08 6A 01 E8 ? ? ? ?"); //441906
		injector::MakeCALL(pattern.get_first(5), sub_490860);
		pattern = hook::pattern("6A 00 E8 ? ? ? ? 6A 01 E8 ? ? ? ? 83"); //4419F4
		injector::MakeCALL(pattern.get_first(2), sub_490860);
	}


	/* Allow 32-bit modes regardless of registry settings - thanks hdc0 */
	if (iniReader.ReadBoolean(INI_KEY, "Allow32Bit", true)) {
		pattern = hook::pattern("74 0B 5E 5D B8 01 00 00 00"); //4ACA44
		injector::WriteMemory<uint8_t>(pattern.get_first(0), '\xEB', true);
	}

	/* Fix "Unable to enumerate a suitable device - thanks hdc0 */
	if (iniReader.ReadBoolean(INI_KEY, "IgnoreVRAM", true)) {
		pattern = hook::pattern("74 44 8B 8A 50 01 00 00 8B 91 64 03 00 00"); //4ACAC2
		injector::WriteMemory<uint8_t>(pattern.get_first(0), '\xEB', true);
	}

	/* Allow copyright/ESRB screen to be skipped immediately */
	if (iniReader.ReadBoolean(INI_KEY, "SkipSplash", true)) {
		pattern = hook::pattern("66 8B 3D ? ? ? ? 83 C4 1C"); //438586
		struct CopyrightHook
		{
			void operator()(injector::reg_pack& regs)
			{
				_asm mov di, 1
			}

		}; injector::MakeInline<CopyrightHook>(pattern.get_first(0), pattern.get_first(7));
	}

	/* Increase Render Distance to Max */
	if (iniReader.ReadBoolean(INI_KEY, "IncreaseRenderDistance", true)) {
		pattern = hook::pattern("D9 44 24 04 D8 4C 24 04 D9 1D"); //4BC410
		float** flt_5088B0_addr = (float**)pattern.get_first(10);
		**flt_5088B0_addr = INFINITY;
		injector::MakeNOP(pattern.get_first(8), 6);
	}

	/* Fix widescreen once game loop begins */
	if (iniReader.ReadBoolean(INI_KEY, "Widescreen", true)) {
		pattern = hook::pattern("8D 44 24 10 50 57 E8 ? ? ? ? 83"); //4317EC
		sub_49D910_addr = ((uintptr_t)pattern.get_first(11) + *pattern.get_first<uintptr_t>(7));
		injector::MakeCALL(pattern.get_first(6), sub_49D910);
	}

	return 0;
}


BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        Init(NULL);
    }
    return TRUE;
}
