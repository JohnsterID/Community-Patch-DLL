// Test stand-in for CivilizationV.exe: loads the test CvGameCore_Expansion2.dll
// and enters it through an EXE-side dispatch table (like the game's listener
// queue). Scenario "exenull" calls a NULL handler from EXE code (eip=0 with
// the caller in the EXE, for which no PDB is available).
#include <windows.h>
#include <intrin.h>
#include <stdio.h>
#include <string.h>

typedef void (*RunFn)(int);
typedef void (*TruthFn)(const char*, void*, void*);
typedef void (*InstallFn)(const char*, const char*);

static TruthFn g_truth = NULL;
static RunFn g_handlers[4] = { NULL, NULL, NULL, NULL };
static volatile int g_sink = 0;

__declspec(noinline) void Dispatch(int slot, int mode)
{
	g_truth("host!Dispatch", _AddressOfReturnAddress(), _ReturnAddress());
	volatile char scratch[256];
	scratch[0] = (char)slot;
	g_handlers[slot + scratch[0] - (char)slot](mode);   // slot 0 is the DLL, slot 1 is NULL
	g_sink += scratch[0];
}

__declspec(noinline) void GameLoop(int slot, int mode)
{
	g_truth("host!GameLoop", _AddressOfReturnAddress(), _ReturnAddress());
	Dispatch(slot, mode);
	g_sink += 1;
}

int main(int argc, char** argv)
{
	const char* scenario = argc > 1 ? argv[1] : "stale";
	const char* config = argc > 2 ? argv[2] : "cfg";
	HMODULE h = LoadLibraryA("CvGameCore_Expansion2.dll");
	if (!h) { printf("LoadLibrary failed %lu\n", GetLastError()); return 1; }
	g_truth = (TruthFn)GetProcAddress(h, "RecordTruth");
	((InstallFn)GetProcAddress(h, "InstallFilter"))(scenario, config);
	g_handlers[0] = (RunFn)GetProcAddress(h, "RunScenario");
	int slot = 0, mode = 1;
	if (!strcmp(scenario, "stale")) mode = 1;
	else if (!strcmp(scenario, "nullcall")) mode = 2;
	else if (!strcmp(scenario, "cxx")) mode = 3;
	else if (!strcmp(scenario, "exenull")) { slot = 1; mode = 0; }
	GameLoop(slot, mode);
	printf("no crash?\n");
	return 0;
}
