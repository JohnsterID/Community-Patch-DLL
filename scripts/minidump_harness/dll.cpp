// Test stand-in for CvGameCore_Expansion2.dll: crash scenarios with a
// recorded ground-truth call chain, plus a minidump-writing exception filter
// using the same MINIDUMP_TYPE flags as the VP Release crash handler.
#include <windows.h>
#include <dbghelp.h>
#include <intrin.h>
#include <stdio.h>
#include <new>

#pragma comment(lib, "dbghelp.lib")

struct TruthFrame { const char* name; void* slot; void* ret; };
static TruthFrame g_truth[64];
static int g_nTruth = 0;
static char g_scenario[64] = "unknown";
static char g_config[16] = "cfg";

extern "C" __declspec(dllexport) void RecordTruth(const char* name, void* slot, void* ret)
{
	if (g_nTruth < 64) { g_truth[g_nTruth].name = name; g_truth[g_nTruth].slot = slot; g_truth[g_nTruth].ret = ret; ++g_nTruth; }
}
#define TRUTH(n) RecordTruth(n, _AddressOfReturnAddress(), _ReturnAddress())

static LONG WINAPI Filter(EXCEPTION_POINTERS* ep)
{
	char path[MAX_PATH];
	sprintf_s(path, "CvMiniDump_%s_%s.dmp", g_scenario, g_config);
	HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION mei;
		mei.ThreadId = GetCurrentThreadId();
		mei.ExceptionPointers = ep;
		mei.ClientPointers = FALSE;
		// VP Release handler flags (CvGlobals.cpp)
		MINIDUMP_TYPE t = (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules |
			MiniDumpWithProcessThreadData | MiniDumpWithHandleData);
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), h, t, &mei, NULL, NULL);
		CloseHandle(h);
	}
	sprintf_s(path, "CvMiniDump_%s_%s.truth.txt", g_scenario, g_config);
	FILE* f = NULL;
	fopen_s(&f, path, "w");
	if (f)
	{
		HMODULE hDll = NULL, hExe = GetModuleHandleA(NULL);
		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&Filter, &hDll);
		fprintf(f, "exception 0x%08lX eip 0x%08lX esp 0x%08lX ebp 0x%08lX dllbase 0x%p exebase 0x%p\n",
			ep->ExceptionRecord->ExceptionCode, ep->ContextRecord->Eip, ep->ContextRecord->Esp, ep->ContextRecord->Ebp, hDll, hExe);
		// innermost first
		for (int i = g_nTruth - 1; i >= 0; --i)
			fprintf(f, "frame %s slot 0x%p ret 0x%p\n", g_truth[i].name, g_truth[i].slot, g_truth[i].ret);
		fclose(f);
	}
	return EXCEPTION_EXECUTE_HANDLER;
}

extern "C" __declspec(dllexport) void InstallFilter(const char* scenario, const char* config)
{
	strcpy_s(g_scenario, scenario);
	strcpy_s(g_config, config);
	SetUnhandledExceptionFilter(Filter);
}

typedef void (*Callback)(int);
struct Listener { int id; Callback cb; };
static Listener* volatile g_listener = NULL;
static int* volatile g_target = NULL;  // opaque NULL: LTO must not turn the write into a trap
static volatile int g_sink = 0;

// --- stale-frame generator: deep recursion leaves return addresses behind ---
__declspec(noinline) int StaleDeep(int n)
{
	volatile int pad[6];
	pad[0] = n;
	if (n > 0)
		return StaleDeep(n - 1) + pad[0];
	return pad[0];
}
__declspec(noinline) int StaleEntry(int n)
{
	volatile int pad[4];
	pad[0] = StaleDeep(n);
	return pad[0];
}

__declspec(noinline) void Live4_Write(int* p, int v)
{
	TRUTH("Live4_Write");
	g_sink += v;
	*p = v;            // scenario "stale": p == NULL -> write AV
	g_sink += *p;
}

__declspec(noinline) void Live3(int mode, int* p)
{
	TRUTH("Live3");
	g_sink += mode;
	if (mode == 1)
		Live4_Write(p, 42);
	else if (mode == 2)
		g_listener->cb(mode);  // listener present, cb NULL -> execute AV at 0
	else if (mode == 3)
		throw std::bad_alloc();
	g_sink += 3;
}

__declspec(noinline) void Live2_BigBuffer(int mode, int* p)
{
	TRUTH("Live2_BigBuffer");
	volatile char buf[12000];  // left uninitialized: stale return addresses survive inside it
	buf[0] = (char)mode;
	buf[sizeof(buf) - 1] = (char)mode;
	Live3(mode + buf[0] - (char)mode, p);
	g_sink += buf[sizeof(buf) - 1];
}

__declspec(noinline) void Live1(int mode, int* p)
{
	TRUTH("Live1");
	Live2_BigBuffer(mode, p);
	g_sink += 1;
}

extern "C" __declspec(dllexport) void RunScenario(int mode)
{
	TRUTH("RunScenario");
	g_sink += StaleEntry(400);     // dirty the stack well below the live frames
	static Listener l = { 7, NULL };
	g_listener = &l;
	Live1(mode, g_target);
}
