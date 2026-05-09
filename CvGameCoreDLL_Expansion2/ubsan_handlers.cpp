// UBSan handlers compatible with VS2008 runtime
// Provides descriptive error messages with value printing and deduplication
// without requiring the UCRT-dependent clang runtime library.
//
// Based on LLVM compiler-rt/lib/ubsan/ but simplified for VS2008 compatibility.
//
// CRITICAL: This entire file must not be instrumented by any sanitizer.
// The handler functions ARE the sanitizer runtime — instrumenting them causes
// infinite recursion (e.g. FNV-1a hash arithmetic triggers unsigned-integer-
// overflow handler which calls the hash again).

#include "CvGameCoreDLLPCH.h"

#ifdef VPDEBUG

// _NO_CVCONST_H exposes SymTagEnum and other DIA declarations from dbghelp.h
// that are normally gated behind the separately-distributed cvconst.h.
#define _NO_CVCONST_H
#include <dbghelp.h>    // types only — all function calls go through dynamic dispatch (see g_dbg below)
#include <psapi.h>      // GetProcessMemoryInfo (process-level VM counters)
#include <TlHelp32.h>   // CreateToolhelp32Snapshot, Module32First/Next (module map)

// Disable all sanitizer instrumentation for every function in this file
#pragma clang attribute push(__attribute__((no_sanitize("undefined", "unsigned-integer-overflow", "implicit-conversion"))), apply_to = function)

// ============================================================================
// Dynamic dbghelp.dll loading — mirror of CvGlobals.cpp LoadBestDbgHelp()
// ============================================================================
//
// WHY: The game ships an ancient dbghelp.dll v6.11 (February 2009) in its
// install directory.  When our DLL statically imports dbghelp.lib, the PE
// loader resolves to this old version.  It has a signed-32-bit range-check
// bug in SymFromAddr (and all address-based APIs) that returns
// ERROR_MOD_NOT_FOUND (126) for any address above 0x80000000 — which is
// where half our game DLL code lives (base 0x7FFF0000, end 0x859F0000).
//
// The minidump code in CvGlobals.cpp already solves this: LoadLibrary from
// GetSystemDirectory() (SysWOW64 on WoW64) gets the modern 32-bit Windows
// dbghelp.dll (v10.x), then GetProcAddress provides function pointers that
// bypass the old static import entirely.
//
// We do exactly the same: load the best available dbghelp.dll, resolve all
// 16 DbgHelp functions through GetProcAddress, and call through g_dbg.*
// everywhere.  The static dbghelp.lib import is still needed for types and
// constants but no function call goes through it.

struct DbgHelpFuncs {
    HMODULE hMod;
    // Symbol engine
    BOOL  (WINAPI *pSymInitialize)(HANDLE, PCSTR, BOOL);
    BOOL  (WINAPI *pSymCleanup)(HANDLE);
    DWORD (WINAPI *pSymSetOptions)(DWORD);
    BOOL  (WINAPI *pSymSetSearchPath)(HANDLE, PCSTR);
    BOOL  (WINAPI *pSymGetSearchPath)(HANDLE, PSTR, DWORD);
    DWORD64 (WINAPI *pSymLoadModuleEx)(HANDLE, HANDLE, PCSTR, PCSTR, DWORD64, DWORD, PMODLOAD_DATA, DWORD);
    // Symbol lookup
    BOOL  (WINAPI *pSymFromAddr)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
    BOOL  (WINAPI *pSymGetLineFromAddr64)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
    BOOL  (WINAPI *pSymGetModuleInfo64)(HANDLE, DWORD64, PIMAGEHLP_MODULE64);
    DWORD64 (WINAPI *pSymGetModuleBase64)(HANDLE, DWORD64);
    // Local variable enumeration
    BOOL  (WINAPI *pSymSetContext)(HANDLE, PIMAGEHLP_STACK_FRAME, PIMAGEHLP_CONTEXT);
    BOOL  (WINAPI *pSymEnumSymbols)(HANDLE, ULONG64, PCSTR, PSYM_ENUMERATESYMBOLS_CALLBACK, PVOID);
    BOOL  (WINAPI *pSymEnumSymbolsForAddr)(HANDLE, DWORD64, PSYM_ENUMERATESYMBOLS_CALLBACK, PVOID);
    BOOL  (WINAPI *pSymGetTypeInfo)(HANDLE, DWORD64, ULONG, IMAGEHLP_SYMBOL_TYPE_INFO, PVOID);
    // Stack walking
    BOOL  (WINAPI *pStackWalk64)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID,
                                  PREAD_PROCESS_MEMORY_ROUTINE64,
                                  PFUNCTION_TABLE_ACCESS_ROUTINE64,
                                  PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
    PVOID (WINAPI *pSymFunctionTableAccess64)(HANDLE, DWORD64);
    // Version
    LPAPI_VERSION (WINAPI *pImagehlpApiVersion)(void);
};

static DbgHelpFuncs g_dbg = {0};

// Load the best available 32-bit dbghelp.dll, mirroring CvGlobals.cpp LoadBestDbgHelp().
// Returns true if all required function pointers were resolved.
static bool loadBestDbgHelp()
{
    if (g_dbg.hMod)
        return g_dbg.pSymInitialize != NULL;

    // Try SysWOW64 first (modern version on Win10/11 64-bit OS).
    // GetSystemDirectory returns SysWOW64 for a 32-bit (WoW64) process.
    TCHAR sysPath[MAX_PATH];
    if (GetSystemDirectory(sysPath, MAX_PATH) > 0) {
        _tcscat_s(sysPath, MAX_PATH, _T("\\dbghelp.dll"));
        g_dbg.hMod = LoadLibrary(sysPath);
    }

    // Fallback: default search order (finds game directory or PATH version).
    if (!g_dbg.hMod)
        g_dbg.hMod = LoadLibrary(_T("dbghelp.dll"));

    if (!g_dbg.hMod)
        return false;

    // Resolve every function.  If any critical one is missing, we can still
    // partially function (stack walk without locals, etc.).
    #define LOAD(name) g_dbg.p##name = (decltype(g_dbg.p##name))GetProcAddress(g_dbg.hMod, #name)
    LOAD(SymInitialize);
    LOAD(SymCleanup);
    LOAD(SymSetOptions);
    LOAD(SymSetSearchPath);
    LOAD(SymGetSearchPath);
    LOAD(SymLoadModuleEx);
    LOAD(SymFromAddr);
    LOAD(SymGetLineFromAddr64);
    LOAD(SymGetModuleInfo64);
    LOAD(SymGetModuleBase64);
    LOAD(SymSetContext);
    LOAD(SymEnumSymbols);
    LOAD(SymEnumSymbolsForAddr);
    LOAD(SymGetTypeInfo);
    LOAD(StackWalk64);
    LOAD(SymFunctionTableAccess64);
    LOAD(ImagehlpApiVersion);
    #undef LOAD

    return g_dbg.pSymInitialize != NULL;
}

// ============================================================================
// Enums from cvconst.h (not present in SDK 7.0a without the DIA SDK)
// These are stable ABI values that have not changed since their introduction.
// ============================================================================

// BasicType — used with TI_GET_BASETYPE to decode scalar local variables.
// Values cross-checked against both microsoft-pdb and wine-mirror cvconst.h.
// Gaps (4, 5, 11, 12, 15–24) are unassigned in both sources.
enum BasicType_e {
    btNoType = 0,  btVoid   = 1,  btChar  = 2,  btWChar  = 3,
    btInt    = 6,  btUInt   = 7,  btFloat = 8,  btBool   = 10,
    btLong   = 13, btULong  = 14,
    btChar16 = 32, btChar32 = 33, btChar8 = 34  // C++11/20 char types
};

// CV_HREG_e x86 subset — used to validate the base register for FRAMEREL locals.
// CV_REG_NONE (0): register unspecified (treat as EBP-relative, safe in debug builds).
// CV_ALLREG_VFRAME (30006): compiler virtual-frame register; equals EBP when FPO is
// disabled (/Oy-), which is always the case in VPDEBUG builds.
enum CV_HREG_x86 {
    CV_REG_NONE     = 0,
    CV_REG_EAX      = 17, CV_REG_ECX = 18, CV_REG_EDX = 19, CV_REG_EBX = 20,
    CV_REG_ESP      = 21, CV_REG_EBP = 22, CV_REG_ESI = 23, CV_REG_EDI = 24,
    CV_ALLREG_VFRAME = 30006  // virtual frame == EBP when /Oy- (FPO off)
};

// ============================================================================
// Type Descriptors (from LLVM ubsan_value.h)
// ============================================================================

struct SourceLocation {
    const char* filename;
    unsigned int line;
    unsigned int column;
};

// Type descriptor - matches LLVM's layout
struct TypeDescriptor {
    unsigned short typeKind;   // 0=integer, 1=float, 0xFFFF=unknown
    unsigned short typeInfo;   // For int: bit 0 = signed, bits 1-15 = bit width log2
    char typeName[1];          // Variable length, null-terminated
    
    bool isInteger() const { return typeKind == 0; }
    bool isFloat() const { return typeKind == 1; }
    bool isSigned() const { return typeInfo & 1; }
    unsigned getIntBitWidth() const { return 1 << (typeInfo >> 1); }
    unsigned getFloatBitWidth() const { return typeInfo; }
};

// Value handle - stores value inline if small, otherwise pointer
typedef uintptr_t ValueHandle;

// ============================================================================
// Data Structures (from LLVM ubsan_handlers.h)
// ============================================================================

struct TypeMismatchData {
    SourceLocation loc;
    const TypeDescriptor* type;
    unsigned char logAlignment;
    unsigned char typeCheckKind;
};

struct OverflowData {
    SourceLocation loc;
    const TypeDescriptor* type;
};

struct ShiftOutOfBoundsData {
    SourceLocation loc;
    const TypeDescriptor* lhsType;
    const TypeDescriptor* rhsType;
};

struct OutOfBoundsData {
    SourceLocation loc;
    const TypeDescriptor* arrayType;
    const TypeDescriptor* indexType;
};

struct UnreachableData {
    SourceLocation loc;
};

struct VLABoundData {
    SourceLocation loc;
    const TypeDescriptor* type;
};

struct InvalidValueData {
    SourceLocation loc;
    const TypeDescriptor* type;
};

struct NonNullArgData {
    SourceLocation loc;
    SourceLocation attrLoc;
    int argIndex;
};

struct NonNullReturnData {
    SourceLocation attrLoc;
};

struct PointerOverflowData {
    SourceLocation loc;
};

// Legacy v1 layout emitted by very old clang (no source location):
//   struct FloatCastOverflowData { const TypeDescriptor *FromType, *ToType; };
// Current v2 layout (with source location) — always emitted by modern clang:
struct FloatCastOverflowDataV2 {
    SourceLocation loc;
    const TypeDescriptor* fromType;
    const TypeDescriptor* toType;
};

struct InvalidBuiltinData {
    SourceLocation loc;
    unsigned char kind;
};

struct FunctionTypeMismatchData {
    SourceLocation loc;
    const TypeDescriptor* type;
};

struct AlignmentAssumptionData {
    SourceLocation loc;
    SourceLocation assumptionLoc;
    const TypeDescriptor* type;
};

struct ImplicitConversionData {
    SourceLocation loc;
    const TypeDescriptor* fromType;
    const TypeDescriptor* toType;
    // ImplicitConversionCheckKind (keep in sync with LLVM CGExprScalar.cpp):
    //   0 = ICCK_IntegerTruncation (legacy clang 7)
    //   1 = ICCK_UnsignedIntegerTruncation
    //   2 = ICCK_SignedIntegerTruncation
    //   3 = ICCK_IntegerSignChange
    //   4 = ICCK_SignedIntegerTruncationOrSignChange
    unsigned char kind;
    unsigned int BitfieldBits; // non-zero when source is a bitfield of this width
};

// ============================================================================
// Deduplication
// ============================================================================
//
// Each UBSan data struct (OverflowData, TypeMismatchData, etc.) is emitted by
// the compiler as a static const local variable — its address is permanently
// unique per source violation site.  Using the data pointer as key gives:
//   • No false collisions (unlike hashing filename+line+col strings)
//   • No string iteration (O(1) instead of O(filename length))
//   • Thread safety via InterlockedCompareExchange (Windows XP+, no CRT needed)
//
// Slot collision (two distinct sites mapping to the same index) means the
// secondary site loses dedup and fires repeatedly — acceptable in a 1024-slot
// table with the handful of active sites a typical debug session produces.

static const size_t DEDUP_TABLE_SIZE = 1024;
static volatile LONG g_dedupTable[DEDUP_TABLE_SIZE];
static volatile LONG g_violationCount = 0;

// Returns true (duplicate — suppress) if this data pointer has already been
// reported. Returns false (new — report) and claims the slot otherwise.
static bool isDuplicate(const void* data)
{
    LONG key   = (LONG)(uintptr_t)data;
    size_t idx = ((uintptr_t)data >> 4) % DEDUP_TABLE_SIZE;
    // CAS: if slot is 0 (empty), write key and return 0 → first report → NOT dup.
    //      if slot already holds key, return key                → IS dup.
    //      if slot holds another key (collision), return other  → NOT dup (report again).
    LONG prev  = InterlockedCompareExchange(&g_dedupTable[idx], key, 0);
    return prev == key;
}

// ============================================================================
// Value Formatting (extract and print actual values)
// ============================================================================

static void formatValue(char* buffer, size_t bufSize, const TypeDescriptor* type, ValueHandle value)
{
    if (!type) {
        sprintf_s(buffer, bufSize, "?");
        return;
    }
    
    if (type->isInteger()) {
        unsigned bits = type->getIntBitWidth();
        if (type->isSigned()) {
            if (bits <= 32) {
                sprintf_s(buffer, bufSize, "%d", (int)(intptr_t)value);
            } else {
                // On 32-bit builds sizeof(ValueHandle)==4, so 64-bit values are passed by pointer
                sprintf_s(buffer, bufSize, "%lld", *(long long*)value);
            }
        } else {
            if (bits <= 32) {
                sprintf_s(buffer, bufSize, "%u", (unsigned int)value);
            } else {
                // On 32-bit builds sizeof(ValueHandle)==4, so 64-bit values are passed by pointer
                sprintf_s(buffer, bufSize, "%llu", *(unsigned long long*)value);
            }
        }
    } else if (type->isFloat()) {
        unsigned bits = type->getFloatBitWidth();
        if (bits <= 32) {
            // Float is passed as bits in the handle
            union { unsigned int i; float f; } u;
            u.i = (unsigned int)value;
            sprintf_s(buffer, bufSize, "%g", u.f);
        } else {
            // Double is passed by pointer
            sprintf_s(buffer, bufSize, "%g", *(double*)value);
        }
    } else {
        sprintf_s(buffer, bufSize, "<unknown>");
    }
}

// ============================================================================
// Core Reporting
// ============================================================================

// ---- Log file ----
// Written to ubsan.log in the process's working directory (same folder as the
// DLL/EXE).  Persists across multiple violation fires in the same session;
// survives debugger output-window clears.

static volatile LONG g_logInitDone = 0;
static FILE*         g_ubsanLog    = NULL;
static DWORD         g_sessionStartTick = 0;

// Game DLL address range captured from the module map at session init.
// Used to identify game-code stack frames by address, so locals diagnostics
// appear even when SymFromAddr cannot resolve symbols (PDB mismatch, wrong
// dbghelp.dll version, module straddling the 2 GB boundary, etc.).
static DWORD g_cvGameDLLBase = 0;
static DWORD g_cvGameDLLEnd  = 0;

// Writes the loaded-module list and process metadata to the log file.
// Called once during session init.
static void writeSessionHeader(FILE* log)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(log, "\n=== UBSan session started ===\n");
    fprintf(log, "  Timestamp : %04u-%02u-%02u %02u:%02u:%02u\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    fprintf(log, "  PID       : %u\n", (unsigned)GetCurrentProcessId());
    fprintf(log, "  Thread    : %u (init)\n", (unsigned)GetCurrentThreadId());

    // Process virtual-memory footprint.
    // GlobalMemoryStatusEx reports system-wide physical RAM, which is irrelevant
    // for a 32-bit process.  GetProcessMemoryInfo gives the actual per-process
    // committed and working-set sizes — the correct signal for memory pressure.
    //
    // The effective VA ceiling depends on /LARGEADDRESSAWARE (LAA) in the EXE
    // header.  Without LAA a 32-bit process gets 2 GB.  With LAA it gets 4 GB
    // on a 64-bit OS (WoW64) or up to 3 GB on a native 32-bit OS (/3GB).
    // We detect LAA by reading the PE header so the ceiling is reported
    // correctly — critical for interpreting the committed-memory numbers below.
    {
        // Detect LAA by reading IMAGE_FILE_LARGE_ADDRESS_AWARE from the EXE header.
        bool laa = false;
        HMODULE hExe = GetModuleHandle(NULL);
        if (hExe) {
            IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)hExe;
            if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
                IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((char*)hExe + dos->e_lfanew);
                if (nt->Signature == IMAGE_NT_SIGNATURE)
                    laa = (nt->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE) != 0;
            }
        }

        // On WoW64 (32-bit process on 64-bit OS) a LAA process gets the full 4 GB.
        // On a native 32-bit OS it gets at most 3 GB (/3GB boot option).
        // Detecting the /3GB boot option would require reading BCD/boot.ini;
        // we just note "32-bit OS" so the reader knows the true limit is unclear.
        unsigned vaCeilingMB = 2048;
        const char* ceilingNote = "no LAA";
        if (laa) {
            BOOL wow64 = FALSE;
            typedef BOOL (WINAPI *PFN_IsWow64Process)(HANDLE, PBOOL);
            PFN_IsWow64Process pfnWow = (PFN_IsWow64Process)
                GetProcAddress(GetModuleHandleA("kernel32.dll"), "IsWow64Process");
            if (pfnWow && pfnWow(GetCurrentProcess(), &wow64) && wow64) {
                vaCeilingMB = 4096;
                ceilingNote = "LAA, 64-bit OS → 4 GB";
            } else {
                vaCeilingMB = 3072;
                ceilingNote = "LAA, 32-bit OS → ≤3 GB";
            }
        }
        fprintf(log, "  VA ceiling: %u MB (%s)\n", vaCeilingMB, ceilingNote);

        PROCESS_MEMORY_COUNTERS pmc;
        pmc.cb = sizeof(pmc);
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            fprintf(log, "  ProcessVM : working set %u MB, committed %u MB (peak %u MB)\n",
                (unsigned)(pmc.WorkingSetSize    / (1024 * 1024)),
                (unsigned)(pmc.PagefileUsage     / (1024 * 1024)),
                (unsigned)(pmc.PeakPagefileUsage / (1024 * 1024)));
        }
    }

    // Module map — lists every DLL/EXE in the process, their base addresses,
    // and sizes.  Essential for offline analysis of stack trace addresses.
    // Also captures the game DLL range (g_cvGameDLLBase/End) so that stack
    // frames can be identified as game-code frames by address alone, without
    // requiring SymFromAddr to succeed.
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 me;
        me.dwSize = sizeof(me);
        fprintf(log, "  Loaded modules:\n");
        if (Module32First(snap, &me)) {
            do {
                DWORD base = (DWORD)(uintptr_t)me.modBaseAddr;
                fprintf(log, "    0x%08X  %7u KB  %s\n",
                    (unsigned)base,
                    (unsigned)(me.modBaseSize / 1024),
                    me.szModule);

                // Capture the game DLL range for address-based frame detection.
                if (strstr(me.szModule, "CvGameCore")) {
                    g_cvGameDLLBase = base;
                    g_cvGameDLLEnd  = base + me.modBaseSize;
                    fprintf(log, "                              ^ game DLL: 0x%08X–0x%08X\n",
                        (unsigned)g_cvGameDLLBase, (unsigned)g_cvGameDLLEnd);
                }
                if (_stricmp(me.szModule, "dbghelp.dll") == 0) {
                    // Flag the process-loaded dbghelp (static import).
                    // Our dynamic dispatch may use a DIFFERENT module — see
                    // "DbgHelp (ubsan)" line after the module table.
                    fprintf(log, "                              ^ process dbghelp (static import)\n");
                }
            } while (Module32Next(snap, &me));
        }
        CloseHandle(snap);
    }

    // Log which dbghelp.dll the ubsan dynamic dispatch actually loaded.
    // This may differ from the process-loaded one shown in the module table
    // above — the whole point of loadBestDbgHelp() is to get the SysWOW64
    // version instead of the ancient game-bundled one.
    if (g_dbg.hMod) {
        char dbgPath[MAX_PATH] = {0};
        GetModuleFileNameA(g_dbg.hMod, dbgPath, MAX_PATH);
        fprintf(log, "\n  DbgHelp (ubsan): %s\n", dbgPath);

        // File version (e.g. 10.0.19041.5848) -- the only way to distinguish
        // old v6.11 from modern v10.x.  API version is 4.0.5 for both.
        {
            DWORD verHandle = 0;
            DWORD verSize = GetFileVersionInfoSizeA(dbgPath, &verHandle);
            if (verSize > 0) {
                char* verBuf = (char*)_alloca(verSize);
                if (GetFileVersionInfoA(dbgPath, verHandle, verSize, verBuf)) {
                    VS_FIXEDFILEINFO* ffi = NULL;
                    UINT ffiLen = 0;
                    if (VerQueryValueA(verBuf, "\\", (void**)&ffi, &ffiLen) && ffi) {
                        fprintf(log, "                   file version %u.%u.%u.%u\n",
                            (unsigned)HIWORD(ffi->dwFileVersionMS),
                            (unsigned)LOWORD(ffi->dwFileVersionMS),
                            (unsigned)HIWORD(ffi->dwFileVersionLS),
                            (unsigned)LOWORD(ffi->dwFileVersionLS));
                    }
                }
            }
        }

        if (g_dbg.pImagehlpApiVersion) {
            LPAPI_VERSION ver = g_dbg.pImagehlpApiVersion();
            if (ver)
                fprintf(log, "                   API version %u.%u.%u (revision %u)\n",
                    (unsigned)ver->MajorVersion, (unsigned)ver->MinorVersion,
                    (unsigned)ver->Revision, (unsigned)ver->Reserved);
        }
    } else {
        fprintf(log, "\n  DbgHelp (ubsan): FAILED TO LOAD -- stack traces will have no symbols\n");
    }

    fprintf(log, "\n");
    fflush(log);
}

static void ubsan_ensure_log()
{
    if (InterlockedCompareExchange(&g_logInitDone, 1, 0) == 0) {
        g_sessionStartTick = GetTickCount();
        // Load the best dbghelp.dll BEFORE writing the session header so
        // we can report which version we got.
        loadBestDbgHelp();
        g_ubsanLog = fopen("ubsan.log", "a");
        if (g_ubsanLog) {
            writeSessionHeader(g_ubsanLog);
        }
    }
}

// ---- DbgHelp initialisation and thread-safety ----
// DbgHelp is not thread-safe.  All calls (SymFromAddr, SymSetContext,
// SymEnumSymbols, SymGetTypeInfo, StackWalk64, …) must be serialised.
// We use a CRITICAL_SECTION initialised on first use with a 3-state guard
// (0=uninit, 1=in-progress, 2=ready) so that the CRITICAL_SECTION itself
// exists before any waiter tries to enter it.

static volatile LONG  g_dbghelpState = 0; // 0 uninit | 1 init-in-progress | 2 ready
static CRITICAL_SECTION g_dbghelpLock;

static void ensureSymInit()
{
    if (InterlockedCompareExchange(&g_dbghelpState, 1, 0) != 0) {
        while (g_dbghelpState < 2) Sleep(0);
        return;
    }

    InitializeCriticalSection(&g_dbghelpLock);

    // loadBestDbgHelp() was already called in ubsan_ensure_log().
    // If it failed, we have no symbol engine at all.
    if (!g_dbg.pSymInitialize) {
        InterlockedExchange(&g_dbghelpState, 2);
        return;
    }

    HANDLE proc = GetCurrentProcess();

    // No SYMOPT_DEFERRED_LOADS -- PDBs must load eagerly during
    // SymLoadModuleEx.  With deferred loading, SymFromAddr resolves export
    // symbols without triggering the PDB load, so private function symbols
    // (the entire game codebase) are never materialized.  The previous run
    // confirmed this: exports like GetTempHeap resolved, but all internal
    // frames returned err=126.
    g_dbg.pSymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

    // invadeProcess=FALSE, then enumerate modules manually via
    // SymLoadModuleEx.  The modern SysWOW64 dbghelp.dll stores module
    // ranges as DWORD64 (unsigned), fixing the signed-32-bit range bug
    // for our DLL straddling 2 GB (base 0x7FFF0000, end 0x859F0000).
    //
    // SymInitialize failure: if another component already called it,
    // clean up and retry (LLVM sanitizer_symbolizer_win.cpp pattern).
    if (!g_dbg.pSymInitialize(proc, NULL, FALSE)) {
        g_dbg.pSymCleanup(proc);
        g_dbg.pSymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
        g_dbg.pSymInitialize(proc, NULL, FALSE);
    }

    // Extend the symbol search path: add this DLL's directory (VS build output,
    // where the PDB is generated) and the EXE directory (game install dir).
    // Source: LLVM sanitizer_symbolizer_win.cpp InitializeDbgHelpIfNeeded().
    {
        char searchPath[2048] = {0};
        g_dbg.pSymGetSearchPath(proc, searchPath, (DWORD)(sizeof(searchPath) - MAX_PATH - 2));

        char dllDir[MAX_PATH] = {0};
        HMODULE hSelf = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)&ensureSymInit, &hSelf) && hSelf) {
            GetModuleFileNameA(hSelf, dllDir, MAX_PATH);
            char* sl = strrchr(dllDir, '\\');
            if (sl) *sl = '\0';
        }

        char exeDir[MAX_PATH] = {0};
        GetModuleFileNameA(NULL, exeDir, MAX_PATH);
        char* sl2 = strrchr(exeDir, '\\');
        if (sl2) *sl2 = '\0';

        size_t used = strlen(searchPath);
        if (dllDir[0]) {
            if (used) searchPath[used++] = ';';
            strncpy_s(searchPath + used, sizeof(searchPath) - used, dllDir, _TRUNCATE);
            used = strlen(searchPath);
        }
        if (exeDir[0] && strcmp(exeDir, dllDir) != 0) {
            if (used) searchPath[used++] = ';';
            strncpy_s(searchPath + used, sizeof(searchPath) - used, exeDir, _TRUNCATE);
        }
        g_dbg.pSymSetSearchPath(proc, searchPath);
    }

    // Register every loaded module via SymLoadModuleEx with explicit DWORD64 base.
    // With invadeProcess=FALSE, DbgHelp has no modules yet; we must add them all.
    // (DWORD64)(DWORD)(uintptr_t)me.modBaseAddr zero-extends the 32-bit pointer
    // to 64 bits, giving DbgHelp a proper unsigned DWORD64 base for range checks.
    if (g_dbg.pSymLoadModuleEx) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            MODULEENTRY32 me;
            me.dwSize = sizeof(me);
            if (Module32First(snap, &me)) {
                do {
                    g_dbg.pSymLoadModuleEx(proc, NULL,
                                    me.szExePath[0]  ? me.szExePath  : NULL,
                                    me.szModule[0]   ? me.szModule   : NULL,
                                    (DWORD64)(DWORD)(uintptr_t)me.modBaseAddr,
                                    me.modBaseSize, NULL, 0);
                } while (Module32Next(snap, &me));
            }
            CloseHandle(snap);
        }
    }

    // Log the symbol search path now that modules are registered.
    if (g_ubsanLog) {
        char finalPath[2048] = {0};
        g_dbg.pSymGetSearchPath(proc, finalPath, sizeof(finalPath));
        fprintf(g_ubsanLog, "  SymPath  : %s\n", finalPath);
        fflush(g_ubsanLog);
    }

    // ---- End-to-end symbol pipeline self-test ----
    // Uses &ensureSymInit as the probe address -- a PRIVATE function in the
    // game DLL whose name we know at compile time.  This exercises the full
    // chain: dbghelp loading, SymInitialize, module registration, PDB load,
    // and both SymFromAddr + SymGetLineFromAddr64.
    //
    // Previous regressions were missed because each diagnostic tested only
    // one link (SymGetModuleBase64 passed but SymFromAddr failed; SymFromAddr
    // passed on an export but PDB private symbols never loaded).  This test
    // requires PDB private symbols to succeed.
    if (g_ubsanLog && g_dbg.pSymFromAddr && g_dbg.pSymGetModuleInfo64) {
        DWORD64 probeAddr = (DWORD64)(DWORD)(uintptr_t)&ensureSymInit;

        // Step 1: module info -- confirms module registered and PDB loaded.
        IMAGEHLP_MODULE64 mi;
        memset(&mi, 0, sizeof(mi));
        mi.SizeOfStruct = sizeof(mi);
        if (g_dbg.pSymGetModuleInfo64(proc, probeAddr, &mi)) {
            const char* symTypeName =
                mi.SymType == SymNone     ? "None"     :
                mi.SymType == SymExport   ? "Exports"  :
                mi.SymType == SymDeferred ? "Deferred" :
                mi.SymType == SymPdb      ? "PDB"      :
                mi.SymType == SymDia      ? "DIA"      :
                mi.SymType == SymCv       ? "CV"       : "Other";
            fprintf(g_ubsanLog, "  DbgHelp  : sym type=%s  pdb=%s\n",
                symTypeName, mi.LoadedPdbName[0] ? mi.LoadedPdbName : "(none)");
        } else {
            fprintf(g_ubsanLog, "  DbgHelp  : SymGetModuleInfo64 failed err=%u"
                " -- module not registered\n", (unsigned)GetLastError());
        }

        // Step 2: SymFromAddr on a known private function.
        char testSymBuf[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO* testSym = (SYMBOL_INFO*)testSymBuf;
        memset(testSymBuf, 0, sizeof(testSymBuf));
        testSym->SizeOfStruct = sizeof(SYMBOL_INFO);
        testSym->MaxNameLen   = 255;

        bool symOK = g_dbg.pSymFromAddr(proc, probeAddr, 0, testSym) != FALSE;
        // A real PDB result will contain "ensureSymInit".  Export-only results
        // return a nearby export name (e.g. "__ubsan_handle_*"), which means
        // the PDB failed to load and only the PE export table is visible.
        bool nameOK = symOK && strstr(testSym->Name, "ensureSymInit") != NULL;

        if (nameOK) {
            fprintf(g_ubsanLog, "  SelfTest : PASS -- SymFromAddr(0x%08X) -> %s"
                " (private PDB symbol)\n",
                (unsigned)(DWORD)probeAddr, testSym->Name);
        } else if (symOK) {
            fprintf(g_ubsanLog, "  SelfTest : FAIL -- SymFromAddr(0x%08X) -> %s"
                " (expected 'ensureSymInit'; got export/wrong symbol"
                " -- PDB private symbols not loaded)\n",
                (unsigned)(DWORD)probeAddr, testSym->Name);
        } else {
            fprintf(g_ubsanLog, "  SelfTest : FAIL -- SymFromAddr(0x%08X) err=%u"
                " (symbol lookup broken entirely)\n",
                (unsigned)(DWORD)probeAddr, (unsigned)GetLastError());
        }

        // Step 3: SymGetLineFromAddr64 -- verifies source line info available.
        if (nameOK && g_dbg.pSymGetLineFromAddr64) {
            IMAGEHLP_LINE64 lineInfo;
            lineInfo.SizeOfStruct = sizeof(lineInfo);
            DWORD lineDisp = 0;
            if (g_dbg.pSymGetLineFromAddr64(proc, probeAddr, &lineDisp, &lineInfo)
                && lineInfo.FileName) {
                fprintf(g_ubsanLog, "  SelfTest : line info -> %s:%u\n",
                    lineInfo.FileName, (unsigned)lineInfo.LineNumber);
            } else {
                fprintf(g_ubsanLog, "  SelfTest : WARN -- SymGetLineFromAddr64 failed"
                    " err=%u (source lines will be missing)\n",
                    (unsigned)GetLastError());
            }
        }

        fflush(g_ubsanLog);
    }

    InterlockedExchange(&g_dbghelpState, 2);
}

// ---- Local variable enumeration ----
// Approach (from debuginfo.com LocalsByAddr + StackWalker):
//   1. RtlCaptureContext + StackWalk64 to get per-frame EBP (unavailable from
//      CaptureStackBackTrace).
//   2. SymSetContext(InstructionOffset=PC) + SymEnumSymbols(module=0, mask=0)
//      to enumerate locals/params in scope for that frame.
//   3. SymGetTypeInfo(TI_GET_BASETYPE / TI_GET_LENGTH) resolves scalar types
//      through typedef chains; structs, arrays and classes are skipped.
//   4. Variable address = EBP + (signed) SYMBOL_INFO.Address for FRAMEREL syms.
//   5. __try/__except guards every memory read — the EBP-relative address may
//      be stale by the time the handler fires.

// Safe memory read — any failure returns false and leaves *out zeroed.
static bool safeRead(DWORD addr, void* out, DWORD size)
{
    __try {
        memcpy(out, (const void*)addr, size);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        memset(out, 0, size);
        return false;
    }
}

// Walk through typedef chains to reach a SymTagBaseType, SymTagPointerType,
// or SymTagEnum.  Returns false for structs, arrays, classes, etc.
static bool resolveTypeTag(HANDLE proc, DWORD64 modBase, DWORD typeIndex,
                           DWORD* outTag, DWORD* outBaseType, ULONG64* outLen)
{
    if (!g_dbg.pSymGetTypeInfo) return false;
    DWORD ti = typeIndex;
    for (int depth = 0; depth < 8; ++depth) {
        DWORD tag = 0;
        if (!g_dbg.pSymGetTypeInfo(proc, modBase, ti, TI_GET_SYMTAG, &tag))
            return false;
        if (tag == SymTagBaseType) {
            DWORD   bt  = btNoType;
            ULONG64 len = 0;
            g_dbg.pSymGetTypeInfo(proc, modBase, ti, TI_GET_BASETYPE, &bt);
            g_dbg.pSymGetTypeInfo(proc, modBase, ti, TI_GET_LENGTH,   &len);
            *outTag = tag; *outBaseType = bt; *outLen = len;
            return true;
        }
        if (tag == SymTagPointerType || tag == SymTagEnum) {
            ULONG64 len = 0;
            g_dbg.pSymGetTypeInfo(proc, modBase, ti, TI_GET_LENGTH, &len);
            *outTag = tag; *outBaseType = btNoType; *outLen = len;
            return true;
        }
        if (tag == SymTagTypedef) {
            DWORD inner = 0;
            if (!g_dbg.pSymGetTypeInfo(proc, modBase, ti, TI_GET_TYPE, &inner))
                return false;
            ti = inner;
            continue;
        }
        return false; // struct, class, array, function pointer, etc.
    }
    return false;
}

struct LocalsCtx {
    char*   buf;
    size_t  bufSize;
    DWORD   frameEBP;
    HANDLE  proc;
    int     count;          // displayable scalars appended so far
    int     limit;          // max displayable scalars per frame
    int     totalReceived;  // all callbacks fired (before any filter) — diagnostic
    // Raw dump of every symbol the callback received, for diagnosing filters.
    // Built as "  sym:'name' Flags=0xNN Reg=N Addr=N Size=N DK=N\n" lines.
    char    rawDump[1024];
    int     rawDumpUsed;
};

static BOOL CALLBACK enumLocalsCallback(PSYMBOL_INFO pSym, ULONG /*symSize*/, PVOID userData)
{
    LocalsCtx* ctx = (LocalsCtx*)userData;
    if (!pSym) return TRUE;
    ++ctx->totalReceived; // count every callback, even ones we filter out

    // --- Raw dump (always, before any filter) ---
    // Records Name, Flags, Register, Address-offset, Size, and DataKind for
    // every symbol the callback receives.  Shown in the log when no displayable
    // scalars are found, so we can see exactly what DbgHelp returned and which
    // filter step rejected it.  Flags key: 0x10=REGREL 0x20=FRAMEREL 0x40=PARAM 0x80=LOCAL.
    // DataKind: 0=Unknown 1=Local 2=StaticLocal 3=Param 4=ObjectPtr 5=FileStatic ...
    DWORD dataKind = 0;
    if (g_dbg.pSymGetTypeInfo)
        g_dbg.pSymGetTypeInfo(ctx->proc, pSym->ModBase, pSym->TypeIndex,
                       TI_GET_DATAKIND, &dataKind);
    if (ctx->rawDumpUsed < (int)sizeof(ctx->rawDump) - 140) {
        int n = sprintf_s(ctx->rawDump + ctx->rawDumpUsed,
                          sizeof(ctx->rawDump) - ctx->rawDumpUsed,
                          "         raw: '%s'  Flags=0x%04X Reg=%u Addr=%+d Size=%u TI=%u DK=%u\n",
                          pSym->Name[0] ? pSym->Name : "(unnamed)",
                          (unsigned)pSym->Flags,
                          (unsigned)pSym->Register,
                          (int)(LONG64)pSym->Address,
                          (unsigned)pSym->Size,
                          (unsigned)pSym->TypeIndex,
                          (unsigned)dataKind);
        if (n > 0) ctx->rawDumpUsed += n;
    }

    if (ctx->count >= ctx->limit) return TRUE;

    // Skip compiler-generated and unnamed symbols ($T1, __$ReturnUdt, etc.)
    if (!pSym->Name[0] || pSym->Name[0] == '$') return TRUE;
    if (pSym->Name[0] == '_' && pSym->Name[1] == '_')  return TRUE;

    // Primary filter: DataKind (from TI_GET_DATAKIND via SymGetTypeInfo).
    // DataIsLocal=1 and DataIsParam=3 are the only kinds we can recover by
    // reading from EBP+offset.  Statics (2,5), object-ptr (4) etc. are skipped.
    //
    // Clang PDBs do not populate TI_GET_DATAKIND (always returns 0).  In that
    // case, fall back to SYMFLAG_LOCAL (0x80) and SYMFLAG_PARAMETER (0x40)
    // which Clang DOES set.  Also accept SYMFLAG_FRAMEREL or SYMFLAG_REGREL
    // with Register=EBP as an additional fallback.
    bool isLocalOrParam = (dataKind == 1 /*DataIsLocal*/ || dataKind == 3 /*DataIsParam*/);
    if (!isLocalOrParam && dataKind == 0) {
        isLocalOrParam = (pSym->Flags & (SYMFLAG_LOCAL | SYMFLAG_PARAMETER)) != 0;
    }
    bool frameRelFallback = false;
    if (!isLocalOrParam && dataKind == 0) {
        frameRelFallback =
            (pSym->Flags & SYMFLAG_FRAMEREL) ||
            ((pSym->Flags & SYMFLAG_REGREL) &&
             (pSym->Register == CV_REG_EBP || pSym->Register == CV_ALLREG_VFRAME));
    }
    if (!isLocalOrParam && !frameRelFallback) return TRUE;

    // For REGREL: verify the base register is EBP-compatible.
    // CV_REG_EBP(22), CV_ALLREG_VFRAME(30006) = EBP when /Oy- (always in VPDEBUG).
    // CV_REG_NONE(0) = unspecified, treat as EBP-relative in debug builds.
    // Any other register (ESP, EAX...) means FPO or enregistered -- skip.
    if (pSym->Flags & SYMFLAG_REGREL) {
        if (pSym->Register != CV_REG_NONE    &&
            pSym->Register != CV_REG_EBP     &&
            pSym->Register != CV_ALLREG_VFRAME) return TRUE;
    }

    // Resolve address: EBP + signed PDB offset (negative for locals below EBP,
    // positive for parameters above EBP).
    DWORD addr = (DWORD)((LONG)ctx->frameEBP + (LONG)pSym->Address);

    // Decode type through typedefs.  If this fails (struct, class, array,
    // or unknown type tag), log the failure reason and skip.
    DWORD tag = 0, baseType = btNoType;
    ULONG64 typeLen = 0;
    if (!resolveTypeTag(ctx->proc, pSym->ModBase, pSym->TypeIndex,
                        &tag, &baseType, &typeLen)) {
        // Log why type resolution failed (first time only, to the log file).
        if (g_ubsanLog && ctx->count == 0 && ctx->totalReceived <= 3) {
            DWORD symTag = 0;
            g_dbg.pSymGetTypeInfo(ctx->proc, pSym->ModBase, pSym->TypeIndex,
                                  TI_GET_SYMTAG, &symTag);
            fprintf(g_ubsanLog, "         [type reject: '%s' TI=%u tag=%u]\n",
                pSym->Name, (unsigned)pSym->TypeIndex, (unsigned)symTag);
        }
        return TRUE;
    }

    DWORD readSize = (DWORD)((typeLen >= 1 && typeLen <= 8) ? typeLen :
                             (pSym->Size >= 1 && pSym->Size <= 8) ? pSym->Size : 4);

    ULONG64 raw = 0;
    bool ok = safeRead(addr, &raw, readSize);

    char valStr[48];
    if (!ok) {
        sprintf_s(valStr, sizeof(valStr), "?");
    } else if (tag == SymTagPointerType) {
        sprintf_s(valStr, sizeof(valStr), "0x%08X", (unsigned)raw);
    } else if (tag == SymTagEnum) {
        sprintf_s(valStr, sizeof(valStr), "%d", (int)(INT32)raw);
    } else {
        switch (baseType) {
        case btBool:
            sprintf_s(valStr, sizeof(valStr), "%s", (raw & 0xFF) ? "true" : "false"); break;
        case btChar: {
            char c = (char)(raw & 0xFF);
            if (c >= 32 && c < 127) sprintf_s(valStr, sizeof(valStr), "%d '%c'", (int)c, c);
            else                    sprintf_s(valStr, sizeof(valStr), "%d", (int)c);
            break;
        }
        case btInt: case btLong:
            if (readSize <= 4) sprintf_s(valStr, sizeof(valStr), "%d",   (int)(INT32)raw);
            else               sprintf_s(valStr, sizeof(valStr), "%lld", *(long long*)&raw);
            break;
        case btUInt: case btULong:
            if (readSize <= 4) sprintf_s(valStr, sizeof(valStr), "%u",   (unsigned)(UINT32)raw);
            else               sprintf_s(valStr, sizeof(valStr), "%llu", (unsigned long long)raw);
            break;
        case btFloat:
            if (readSize <= 4) {
                union { unsigned u; float f; } uf; uf.u = (unsigned)(UINT32)raw;
                sprintf_s(valStr, sizeof(valStr), "%g", uf.f);
            } else {
                sprintf_s(valStr, sizeof(valStr), "%g", *(double*)&raw);
            }
            break;
        default:
            return TRUE; // wchar_t, void, etc. — not useful to display
        }
    }

    // Append " name=value" to the locals scratch buffer
    char entry[128];
    sprintf_s(entry, sizeof(entry), " %s=%s", pSym->Name, valStr);
    size_t used = strlen(ctx->buf);
    size_t elen = strlen(entry);
    if (used + elen < ctx->bufSize) {
        memcpy(ctx->buf + used, entry, elen);
        ctx->buf[used + elen] = '\0';
    }
    ++ctx->count;
    return TRUE;
}

// Enumerate and append locals for one stack frame.
// SymSetContext + SymEnumSymbols must be called inside g_dbghelpLock (already held).
// Always appends a "locals:" line — either values or a bracketed diagnostic — so
// the log always tells us what happened rather than silently producing nothing.
static void appendLocalsForFrame(char* buf, size_t bufSize,
                                  HANDLE proc, DWORD64 pc, DWORD frameEBP)
{
    char localsBuf[512];
    localsBuf[0] = '\0';

    // Build the line into a scratch buffer first; commit to buf only at the end.
    // Large enough to hold the summary line + up to ~10 raw symbol dump lines.
    char line[1800];

    if (frameEBP == 0) {
        sprintf_s(line, sizeof(line), "         locals: [ebp=0 — StackWalk64 did not provide frame pointer]\n");
        goto commit;
    }

    {
        LocalsCtx ctx;
        ctx.buf           = localsBuf;
        ctx.bufSize       = sizeof(localsBuf);
        ctx.frameEBP      = frameEBP;
        ctx.proc          = proc;
        ctx.count         = 0;
        ctx.limit         = 8;
        ctx.totalReceived = 0;
        ctx.rawDump[0]    = '\0';
        ctx.rawDumpUsed   = 0;

        // Method 1: SymSetContext + SymEnumSymbols(module=0, mask=0).
        // This is the LocalsByAddr.cpp (debuginfo.com) pattern.  Requires the PC
        // to be within a function known to DbgHelp (PDB loaded and matched).
        // If this fails the most common causes are: old dbghelp.dll (< 6.x),
        // PDB not loaded, or PC outside any function symbol.
        {
            IMAGEHLP_STACK_FRAME isf;
            memset(&isf, 0, sizeof(isf));
            isf.InstructionOffset = pc;
            if (!g_dbg.pSymSetContext || !g_dbg.pSymSetContext(proc, &isf, 0)) {
                DWORD err = GetLastError();
                sprintf_s(line, sizeof(line),
                    "         locals: [SymSetContext failed err=%u pc=0x%08X ebp=0x%08X"
                    " — try method2]\n",
                    (unsigned)err, (unsigned)(DWORD)pc, (unsigned)frameEBP);
                // Fall through to method 2 below.
            } else if (g_dbg.pSymEnumSymbols) {
                g_dbg.pSymEnumSymbols(proc, 0, 0, enumLocalsCallback, &ctx);
            }
        }

        // Method 2 fallback: SymEnumSymbolsForAddr(proc, pc, cb, ctx).
        // Available since DbgHelp 6.2 (included in v7.0a SDK).  Takes just an
        // address, no prior SymSetContext call required.  Enumerates all symbols
        // whose scope includes 'pc'.  Try this if method 1 found nothing.
        if (ctx.totalReceived == 0 && g_dbg.pSymEnumSymbolsForAddr) {
            g_dbg.pSymEnumSymbolsForAddr(proc, pc, enumLocalsCallback, &ctx);
        }

        if (ctx.count > 0) {
            // Normal case: at least one displayable scalar found.
            sprintf_s(line, sizeof(line), "         locals:%s\n", localsBuf);
        } else if (ctx.totalReceived == 0) {
            // Neither method fired any callbacks — most likely the PDB has no
            // private symbol info at this PC (no locals in PDB scope, wrong PDB
            // GUID, or dbghelp version too old for local enumeration entirely).
            sprintf_s(line, sizeof(line),
                "         locals: [none — both SymEnumSymbols and SymEnumSymbolsForAddr"
                " returned 0 symbols]\n");
        } else {
            // Symbols found but all filtered.  Include the raw dump so the reader
            // can see what DbgHelp returned and why each was rejected.
            // (DK=1/3 = local/param, DK=0 = type info unavailable, Flags 0x20 = FRAMEREL)
            int headerLen = sprintf_s(line, sizeof(line),
                "         locals: [%d symbol(s) found, 0 displayable"
                " (all non-scalar/non-EBP-relative); raw dump:]\n",
                ctx.totalReceived);
            // Append the per-symbol raw dump lines while ctx is still in scope.
            if (headerLen > 0 && ctx.rawDumpUsed > 0) {
                size_t space = sizeof(line) - (size_t)headerLen - 1;
                size_t rlen  = (size_t)ctx.rawDumpUsed;
                if (rlen > space) rlen = space;
                memcpy(line + headerLen, ctx.rawDump, rlen);
                line[headerLen + rlen] = '\0';
            }
        }
    }

commit:
    {
        size_t used = strlen(buf);
        size_t llen = strlen(line);
        if (used + llen < bufSize) {
            memcpy(buf + used, line, llen);
            buf[used + llen] = '\0';
        }
    }
}

// ---- Stack trace with local variables ----
// Uses StackWalk64 so that every frame carries its EBP (AddrFrame.Offset),
// which is needed to compute the address of EBP-relative locals.
//
// x86 CONTEXT CAPTURE — do NOT use RtlCaptureContext on x86.
// RtlCaptureContext may record the wrong EIP/EBP for the calling frame,
// causing StackWalk64 to heuristically scan the stack for code pointers
// rather than following the true EBP chain.  The resulting addresses look
// plausible (they are real instruction bytes in the DLL) but are NOT valid
// return sites, so SymFromAddr rejects them.
// Use the same inline-asm approach as StackWalker (StackWalker.h,
// GET_CURRENT_CONTEXT_STACKWALKER_CODEPLEX): call/pop captures the real EIP,
// and reading ebp/esp directly gives the caller's true frame and stack ptrs.
// (LLVM sanitizer_common avoids StackWalk64 for this reason and falls back
// to CaptureStackBackTrace for current-thread captures.)
//
// Frame skip logic (matching old CaptureStackBackTrace(3,...) output):
//   rawFrame 0 = appendStackTrace  (skip — internal)
//   rawFrame 1 = ubsan_output      (skip — internal)
//   rawFrame 2 = ubsan_report* / impl_*  → displayed as #0
//   rawFrame 3 = __ubsan_handle_*       → displayed as #1
//   rawFrame 4+ = game code             → displayed with locals
//
// Locals are shown for frames in the game DLL address range (g_cvGameDLLBase).
// Up to MAX_LOCAL_FRAMES frames get locals; up to 8 scalars each.
static void appendStackTrace(char* buf, size_t bufSize)
{
    ensureSymInit();

    // If we couldn't load any dbghelp.dll, no stack trace is possible.
    if (!g_dbg.pStackWalk64) {
        const char* msg = "  Stack: [dbghelp.dll not loaded — no stack trace]\n";
        size_t used = strlen(buf);
        size_t mlen = strlen(msg);
        if (used + mlen < bufSize) { memcpy(buf + used, msg, mlen); buf[used + mlen] = '\0'; }
        return;
    }

    CONTEXT ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_FULL;

    // x86 inline-asm context capture (StackWalker approach).
    // call/pop gives the real EIP; ebp and esp are read directly.
    // This is reliable on all x86 Windows versions including XP.
    __asm {
        call L
        L: pop eax
        mov ctx.Eip, eax
        mov ctx.Ebp, ebp
        mov ctx.Esp, esp
    }

    STACKFRAME64 sf;
    memset(&sf, 0, sizeof(sf));
    sf.AddrPC.Offset    = ctx.Eip;  sf.AddrPC.Mode    = AddrModeFlat;
    sf.AddrFrame.Offset = ctx.Ebp;  sf.AddrFrame.Mode = AddrModeFlat;
    sf.AddrStack.Offset = ctx.Esp;  sf.AddrStack.Mode = AddrModeFlat;

    char symBuf[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO* sym = (SYMBOL_INFO*)symBuf;

    IMAGEHLP_LINE64 lineInfo;
    lineInfo.SizeOfStruct = sizeof(lineInfo);

    HANDLE proc   = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    size_t used = strlen(buf);
    const char* hdr = "  Stack:\n";
    size_t hlen = strlen(hdr);
    if (used + hlen < bufSize) {
        memcpy(buf + used, hdr, hlen);
        used += hlen;
        buf[used] = '\0';
    }

    EnterCriticalSection(&g_dbghelpLock);

    // Two-pass stack trace: collect PCs first, resolve symbols second.
    // StackWalk64 modifies internal DbgHelp state (function table access,
    // module base lookups) that can interfere with SymFromAddr when called
    // inside the walk loop.  Previous run confirmed this: SelfTest passed
    // (SymFromAddr on &ensureSymInit before walk), but every SymFromAddr
    // inside the StackWalk64 loop returned err=126 on the same addresses.

    static const int SKIP_INTERNAL = 2; // appendStackTrace + ubsan_output
    static const int MAX_FRAMES    = 48;
    static const int MAX_LOCAL_FRAMES = 48;

    struct FrameInfo { DWORD64 pc; DWORD ebp; };
    FrameInfo frames[MAX_FRAMES];
    int frameCount = 0;
    int rawFrame   = 0;

    // Pass 1: walk the stack, collect PCs and frame pointers.
    // CRITICAL: truncate AddrPC.Offset to 32 bits.  StackWalk64 for
    // IMAGE_FILE_MACHINE_I386 sign-extends 32-bit EIP values into the
    // 64-bit Offset field.  Addresses >= 0x80000000 become
    // 0xFFFFFFFF8XXXXXXX, which no registered module covers -- causing
    // SymFromAddr / SymGetModuleBase64 to return "module not found" (126).
    while (g_dbg.pStackWalk64(IMAGE_FILE_MACHINE_I386, proc, thread, &sf, &ctx,
                       NULL, g_dbg.pSymFunctionTableAccess64, g_dbg.pSymGetModuleBase64, NULL))
    {
        if (sf.AddrPC.Offset == 0) break;
        if (rawFrame < SKIP_INTERNAL) { ++rawFrame; continue; }
        if (frameCount >= MAX_FRAMES) break;
        frames[frameCount].pc  = (DWORD64)(DWORD)sf.AddrPC.Offset;
        frames[frameCount].ebp = (DWORD)sf.AddrFrame.Offset;
        ++frameCount;
        ++rawFrame;
    }

    // Pass 2: resolve symbols and locals (StackWalk64 is finished).
    int resolvedCount = 0;
    int localsShown   = 0;
    int symFailCount  = 0;

    for (int i = 0; i < frameCount; ++i) {
        if (used + 480 >= bufSize) break;

        memset(symBuf, 0, sizeof(symBuf));
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen   = 255;

        DWORD64     pc        = frames[i].pc;
        DWORD       lineDisp  = 0;
        const char* srcFile   = NULL;
        char        frameLine[320];

        if (g_dbg.pSymFromAddr && g_dbg.pSymFromAddr(proc, pc, 0, sym)) {
            ++resolvedCount;
            if (g_dbg.pSymGetLineFromAddr64 && g_dbg.pSymGetLineFromAddr64(proc, pc, &lineDisp, &lineInfo)) {
                srcFile = lineInfo.FileName;
                sprintf_s(frameLine, sizeof(frameLine), "    #%-2u  %s  (%s:%u)\n",
                    (unsigned)i, sym->Name,
                    lineInfo.FileName, (unsigned)lineInfo.LineNumber);
            } else {
                sprintf_s(frameLine, sizeof(frameLine), "    #%-2u  %s\n",
                    (unsigned)i, sym->Name);
            }
        } else {
            if (symFailCount < 2) {
                DWORD err = GetLastError();
                sprintf_s(frameLine, sizeof(frameLine),
                    "    #%-2u  0x%08X  [SymFromAddr err=%u]\n",
                    (unsigned)i, (unsigned)(DWORD)pc, (unsigned)err);
                ++symFailCount;
            } else {
                sprintf_s(frameLine, sizeof(frameLine), "    #%-2u  0x%08X\n",
                    (unsigned)i, (unsigned)(DWORD)pc);
            }
        }

        size_t flen = strlen(frameLine);
        if (used + flen < bufSize) {
            memcpy(buf + used, frameLine, flen);
            used += flen;
            buf[used] = '\0';
        }

        DWORD pcDword = (DWORD)(ULONG_PTR)pc;
        bool isGameFrame;
        if (g_cvGameDLLBase != 0) {
            isGameFrame = (pcDword >= g_cvGameDLLBase && pcDword < g_cvGameDLLEnd);
        } else {
            isGameFrame = srcFile && !strstr(srcFile, "ubsan_handlers");
        }

        if (isGameFrame && localsShown < MAX_LOCAL_FRAMES) {
            DWORD ebp = frames[i].ebp;
            if (used + 1800 < bufSize) {
                appendLocalsForFrame(buf, bufSize, proc, pc, ebp);
                used = strlen(buf);
            }
            if (ebp != 0) ++localsShown;
        }
    }

    if (frameCount > 0 && used + 80 < bufSize) {
        char summary[80];
        sprintf_s(summary, sizeof(summary), "    [%d/%d frames resolved]\n",
            resolvedCount, frameCount);
        size_t slen = strlen(summary);
        memcpy(buf + used, summary, slen);
        used += slen;
        buf[used] = '\0';
    }

    LeaveCriticalSection(&g_dbghelpLock);
}

// ---- Source context ----
// Reads ±2 lines around the violation line from the source file and appends
// them to buf.  The source path embedded in the binary is the absolute path
// from the build machine; fopen silently fails on a different machine, in
// which case this is a clean no-op.  No extra dependencies required.
static void appendSourceContext(char* buf, size_t bufSize, const char* filename, unsigned int line)
{
    if (!filename || !filename[0] || line == 0) return;

    FILE* f = fopen(filename, "r");
    if (!f) return;

    static const unsigned CONTEXT = 2;
    unsigned firstLine = (line > CONTEXT) ? line - CONTEXT : 1;
    unsigned lastLine  = line + CONTEXT;

    char srcLine[256];
    unsigned curLine = 0;
    size_t used = strlen(buf);

    const char* hdr = "  Source:\n";
    size_t hlen = strlen(hdr);
    if (used + hlen < bufSize) {
        memcpy(buf + used, hdr, hlen);
        used += hlen;
        buf[used] = '\0';
    }

    while (fgets(srcLine, sizeof(srcLine), f) && curLine <= lastLine) {
        ++curLine;
        if (curLine < firstLine) continue;

        // Strip trailing newline/carriage-return
        size_t slen = strlen(srcLine);
        while (slen > 0 && (srcLine[slen - 1] == '\n' || srcLine[slen - 1] == '\r'))
            srcLine[--slen] = '\0';

        char formatted[320];
        if (curLine == line)
            sprintf_s(formatted, sizeof(formatted), "  >%4u: %s\n", curLine, srcLine);
        else
            sprintf_s(formatted, sizeof(formatted), "   %4u: %s\n", curLine, srcLine);

        size_t flen = strlen(formatted);
        if (used + flen < bufSize) {
            memcpy(buf + used, formatted, flen);
            used += flen;
            buf[used] = '\0';
        }
    }
    fclose(f);
}

// ---- Output ----
// Two-tier output: the debugger output window gets a concise one-liner (the
// developer already has the full call stack, locals, and watch in the IDE).
// The log file and stderr get the full enriched dump with stack traces,
// memory descriptors, timestamps, and sequence numbers — this is what you
// rely on when no debugger is attached.

static void ubsan_output(const char* message)
{
    ubsan_ensure_log();

    LONG seq = InterlockedIncrement(&g_violationCount);
    DWORD elapsed = GetTickCount() - g_sessionStartTick;
    DWORD tid = GetCurrentThreadId();

    // ---- Full enriched output (log file + stderr) ----
    // 32 KB accommodates source context + 32 frames + up to 4×8 scalar locals.
    char full[32768];
    sprintf_s(full, sizeof(full), "[#%ld T+%u.%03us TID:%u] %s",
        seq,
        elapsed / 1000, elapsed % 1000,
        (unsigned)tid,
        message);
    appendStackTrace(full, sizeof(full));

    fprintf(stderr, "%s", full);
    fflush(stderr);
    if (g_ubsanLog) {
        fprintf(g_ubsanLog, "%s", full);
        fflush(g_ubsanLog);
    }

    // ---- Concise output (debugger output window) ----
    // When a debugger is attached the IDE provides the call stack, locals,
    // and watch windows — the enriched dump is redundant noise.  Send just
    // the violation headline so the Output pane stays scannable.
    if (IsDebuggerPresent()) {
        char brief[512];
        sprintf_s(brief, sizeof(brief), "[UBSan #%ld] %s", seq, message);
        OutputDebugStringA(brief);
    }
}

// Break into the debugger only when one is attached.
// Used by recoverable handlers so the game continues running without a debugger
// (accumulating all violations in the log) while still breaking when debugging.
static void ubsan_break()
{
    if (IsDebuggerPresent())
        __debugbreak();
}

// ---- Memory region description (for pointer violations) ----
// Queries VirtualQuery to describe the memory state at an address.
// Returns a human-readable string like "COMMIT RW (private, 4096 B)"
// or "FREE" / "RESERVE".  Helps diagnose null, dangling, or wild pointers
// without a debugger.

static void describeMemoryRegion(char* out, size_t outSize, uintptr_t addr)
{
    if (addr == 0) {
        sprintf_s(out, outSize, "NULL");
        return;
    }

    MEMORY_BASIC_INFORMATION mbi;
    memset(&mbi, 0, sizeof(mbi));
    if (!VirtualQuery((const void*)addr, &mbi, sizeof(mbi))) {
        sprintf_s(out, outSize, "VirtualQuery failed (err %u)", (unsigned)GetLastError());
        return;
    }

    const char* state;
    switch (mbi.State) {
    case MEM_COMMIT:  state = "COMMIT";  break;
    case MEM_RESERVE: state = "RESERVE"; break;
    case MEM_FREE:    state = "FREE";    break;
    default:          state = "?";       break;
    }

    // Protection flags (only meaningful when committed)
    char prot[32];
    prot[0] = '\0';
    if (mbi.State == MEM_COMMIT) {
        DWORD p = mbi.Protect;
        if (p & PAGE_NOACCESS)          sprintf_s(prot, sizeof(prot), " NOACCESS");
        else if (p & PAGE_READONLY)     sprintf_s(prot, sizeof(prot), " R");
        else if (p & PAGE_READWRITE)    sprintf_s(prot, sizeof(prot), " RW");
        else if (p & PAGE_EXECUTE_READ) sprintf_s(prot, sizeof(prot), " RX");
        else if (p & PAGE_EXECUTE_READWRITE) sprintf_s(prot, sizeof(prot), " RWX");
        else if (p & PAGE_EXECUTE)      sprintf_s(prot, sizeof(prot), " X");
        else                            sprintf_s(prot, sizeof(prot), " prot=0x%X", (unsigned)p);
        if (p & PAGE_GUARD) {
            size_t len = strlen(prot);
            sprintf_s(prot + len, sizeof(prot) - len, "+GUARD");
        }
    }

    const char* mtype;
    switch (mbi.Type) {
    case MEM_IMAGE:   mtype = "image";   break;
    case MEM_MAPPED:  mtype = "mapped";  break;
    case MEM_PRIVATE: mtype = "private"; break;
    default:          mtype = "";        break;
    }

    sprintf_s(out, outSize, "%s%s (%s, %u B region)",
        state, prot, mtype, (unsigned)mbi.RegionSize);
}

// Returns true if the violation was newly reported (caller should break).
// Returns false if it was a duplicate (caller should silently continue).
static bool ubsan_report(const void* key, const char* errorType, const SourceLocation& loc)
{
    if (isDuplicate(key)) return false;

    char buffer[2048];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: %s ***\n    at %s:%u:%u\n",
        errorType,
        loc.filename ? loc.filename : "<unknown>",
        loc.line,
        loc.column);
    appendSourceContext(buffer, sizeof(buffer), loc.filename, loc.line);

    ubsan_output(buffer);
    return true;
}

static bool ubsan_report_with_value(const void* key, const char* errorType, const SourceLocation& loc,
                                     const char* desc, const TypeDescriptor* type, ValueHandle value)
{
    if (isDuplicate(key)) return false;

    char valStr[64];
    formatValue(valStr, sizeof(valStr), type, value);

    char buffer[2048];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: %s ***\n    %s: %s (type: %s)\n    at %s:%u:%u\n",
        errorType,
        desc,
        valStr,
        type ? type->typeName : "<unknown>",
        loc.filename ? loc.filename : "<unknown>",
        loc.line,
        loc.column);
    appendSourceContext(buffer, sizeof(buffer), loc.filename, loc.line);

    ubsan_output(buffer);
    return true;
}

static bool ubsan_report_overflow(const void* key, const char* op, const SourceLocation& loc,
                                   const TypeDescriptor* type, ValueHandle lhs, ValueHandle rhs)
{
    if (isDuplicate(key)) return false;

    char lhsStr[64], rhsStr[64];
    formatValue(lhsStr, sizeof(lhsStr), type, lhs);
    formatValue(rhsStr, sizeof(rhsStr), type, rhs);

    char buffer[2048];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: %s overflow ***\n    %s %s %s cannot be represented in type %s\n    at %s:%u:%u\n",
        type && type->isSigned() ? "signed integer" : "unsigned integer",
        lhsStr, op, rhsStr,
        type ? type->typeName : "<unknown>",
        loc.filename ? loc.filename : "<unknown>",
        loc.line,
        loc.column);
    appendSourceContext(buffer, sizeof(buffer), loc.filename, loc.line);

    ubsan_output(buffer);
    return true;
}

// ============================================================================
// Handler Implementations (extern "C" for linker compatibility)
// ============================================================================

extern "C" {

// ---- Type mismatch ----

static const char* getTypeCheckKindName(unsigned char kind)
{
    static const char* names[] = {
        "load of", "store to", "reference binding to", "member access within",
        "member call on", "constructor call on", "downcast of", "downcast of",
        "upcast of", "cast to virtual base of", "_Nonnull binding to",
        "dynamic operation on"
    };
    return kind < sizeof(names)/sizeof(names[0]) ? names[kind] : "access of";
}

static bool impl_type_mismatch(TypeMismatchData* data, ValueHandle pointer)
{
    if (isDuplicate(data)) return false;
    const char* typeName  = data->type ? data->type->typeName : "<unknown>";
    const char* checkKind = getTypeCheckKindName(data->typeCheckKind);

    char memDesc[128];
    describeMemoryRegion(memDesc, sizeof(memDesc), (uintptr_t)pointer);

    char buffer[2048];
    if (!pointer) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: null pointer access ***\n    %s null pointer of type %s\n    at %s:%u:%u\n",
            checkKind, typeName, data->loc.filename, data->loc.line, data->loc.column);
    } else if ((pointer & ((1u << data->logAlignment) - 1u)) != 0) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: misaligned address ***\n    %s misaligned address 0x%p for type %s (requires %u-byte alignment)\n    memory: %s\n    at %s:%u:%u\n",
            checkKind, (void*)pointer, typeName, 1u << data->logAlignment,
            memDesc,
            data->loc.filename, data->loc.line, data->loc.column);
    } else {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: type mismatch ***\n    %s address 0x%p with insufficient space for type %s\n    memory: %s\n    at %s:%u:%u\n",
            checkKind, (void*)pointer, typeName,
            memDesc,
            data->loc.filename, data->loc.line, data->loc.column);
    }
    appendSourceContext(buffer, sizeof(buffer), data->loc.filename, data->loc.line);
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_type_mismatch_v1(TypeMismatchData* data, ValueHandle pointer)
{
    if (impl_type_mismatch(data, pointer)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_type_mismatch_v1_abort(TypeMismatchData* data, ValueHandle pointer)
{
    if (impl_type_mismatch(data, pointer)) __debugbreak();
}

// ---- Integer overflow ----

__declspec(dllexport) void __ubsan_handle_add_overflow(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (ubsan_report_overflow(data, "+", data->loc, data->type, lhs, rhs)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_add_overflow_abort(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (ubsan_report_overflow(data, "+", data->loc, data->type, lhs, rhs)) __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_sub_overflow(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (ubsan_report_overflow(data, "-", data->loc, data->type, lhs, rhs)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_sub_overflow_abort(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (ubsan_report_overflow(data, "-", data->loc, data->type, lhs, rhs)) __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_mul_overflow(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (ubsan_report_overflow(data, "*", data->loc, data->type, lhs, rhs)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_mul_overflow_abort(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (ubsan_report_overflow(data, "*", data->loc, data->type, lhs, rhs)) __debugbreak();
}

static bool impl_divrem_overflow(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (isDuplicate(data)) return false;
    char lhsStr[64], rhsStr[64];
    formatValue(lhsStr, sizeof(lhsStr), data->type, lhs);
    formatValue(rhsStr, sizeof(rhsStr), data->type, rhs);
    char buffer[2048];
    if (rhs == 0) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: division by zero ***\n    %s / 0 is undefined\n    at %s:%u:%u\n",
            lhsStr, data->loc.filename, data->loc.line, data->loc.column);
    } else {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: division overflow ***\n    %s / %s cannot be represented in type %s\n    at %s:%u:%u\n",
            lhsStr, rhsStr, data->type ? data->type->typeName : "<unknown>",
            data->loc.filename, data->loc.line, data->loc.column);
    }
    appendSourceContext(buffer, sizeof(buffer), data->loc.filename, data->loc.line);
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_divrem_overflow(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (impl_divrem_overflow(data, lhs, rhs)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_divrem_overflow_abort(OverflowData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (impl_divrem_overflow(data, lhs, rhs)) __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_negate_overflow(OverflowData* data, ValueHandle val)
{
    if (ubsan_report_with_value(data, "negation overflow", data->loc, "cannot negate", data->type, val)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_negate_overflow_abort(OverflowData* data, ValueHandle val)
{
    if (ubsan_report_with_value(data, "negation overflow", data->loc, "cannot negate", data->type, val)) __debugbreak();
}

// ---- Shift errors ----

static bool impl_shift_out_of_bounds(ShiftOutOfBoundsData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (isDuplicate(data)) return false;
    char lhsStr[64], rhsStr[64];
    formatValue(lhsStr, sizeof(lhsStr), data->lhsType, lhs);
    formatValue(rhsStr, sizeof(rhsStr), data->rhsType, rhs);
    char buffer[2048];
    if (data->lhsType && data->lhsType->isInteger()) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: shift out of bounds ***\n    shift amount %s is invalid for %u-bit type %s (value: %s)\n    at %s:%u:%u\n",
            rhsStr, data->lhsType->getIntBitWidth(), data->lhsType->typeName, lhsStr,
            data->loc.filename, data->loc.line, data->loc.column);
    } else {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: shift out of bounds ***\n    shift amount %s is invalid for type %s (value: %s)\n    at %s:%u:%u\n",
            rhsStr, data->lhsType ? data->lhsType->typeName : "<unknown>", lhsStr,
            data->loc.filename, data->loc.line, data->loc.column);
    }
    appendSourceContext(buffer, sizeof(buffer), data->loc.filename, data->loc.line);
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_shift_out_of_bounds(ShiftOutOfBoundsData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (impl_shift_out_of_bounds(data, lhs, rhs)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_shift_out_of_bounds_abort(ShiftOutOfBoundsData* data, ValueHandle lhs, ValueHandle rhs)
{
    if (impl_shift_out_of_bounds(data, lhs, rhs)) __debugbreak();
}

// ---- Array bounds ----

__declspec(dllexport) void __ubsan_handle_out_of_bounds(OutOfBoundsData* data, ValueHandle index)
{
    if (ubsan_report_with_value(data, "array index out of bounds", data->loc, "index", data->indexType, index)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_out_of_bounds_abort(OutOfBoundsData* data, ValueHandle index)
{
    if (ubsan_report_with_value(data, "array index out of bounds", data->loc, "index", data->indexType, index)) __debugbreak();
}

// ---- Unreachable / missing return (UNRECOVERABLE -- always fatal) ----

__declspec(dllexport) void __ubsan_handle_builtin_unreachable(UnreachableData* data)
{
    if (ubsan_report(data, "execution reached __builtin_unreachable()", data->loc))
        __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_builtin_unreachable_abort(UnreachableData* data)
{
    __ubsan_handle_builtin_unreachable(data);
}

__declspec(dllexport) void __ubsan_handle_missing_return(UnreachableData* data)
{
    if (ubsan_report(data, "execution reached end of non-void function without returning a value", data->loc))
        __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_missing_return_abort(UnreachableData* data)
{
    __ubsan_handle_missing_return(data);
}

// ---- VLA bound ----

__declspec(dllexport) void __ubsan_handle_vla_bound_not_positive(VLABoundData* data, ValueHandle bound)
{
    if (ubsan_report_with_value(data, "variable length array bound is not positive", data->loc, "bound", data->type, bound)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_vla_bound_not_positive_abort(VLABoundData* data, ValueHandle bound)
{
    if (ubsan_report_with_value(data, "variable length array bound is not positive", data->loc, "bound", data->type, bound)) __debugbreak();
}

// ---- Float cast overflow ----
// Takes void* to accommodate both legacy v1 (no SourceLocation) and current v2
// (with SourceLocation) layouts.  Modern clang always emits v2.

static bool impl_float_cast_overflow(void* dataPtr, ValueHandle val)
{
    FloatCastOverflowDataV2* data = (FloatCastOverflowDataV2*)dataPtr;
    if (isDuplicate(data)) return false;
    char valStr[64];
    formatValue(valStr, sizeof(valStr), data->fromType, val);
    char buffer[2048];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: float cast overflow ***\n    %s (type %s) is outside the range of type %s\n    at %s:%u:%u\n",
        valStr,
        data->fromType ? data->fromType->typeName : "<unknown>",
        data->toType   ? data->toType->typeName   : "<unknown>",
        data->loc.filename, data->loc.line, data->loc.column);
    appendSourceContext(buffer, sizeof(buffer), data->loc.filename, data->loc.line);
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_float_cast_overflow(void* data, ValueHandle val)
{
    if (impl_float_cast_overflow(data, val)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_float_cast_overflow_abort(void* data, ValueHandle val)
{
    if (impl_float_cast_overflow(data, val)) __debugbreak();
}

// ---- Load of invalid value (bad bool / unscoped enum) ----

__declspec(dllexport) void __ubsan_handle_load_invalid_value(InvalidValueData* data, ValueHandle val)
{
    if (ubsan_report_with_value(data, "load of value outside valid range for type", data->loc, "value", data->type, val)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_load_invalid_value_abort(InvalidValueData* data, ValueHandle val)
{
    if (ubsan_report_with_value(data, "load of value outside valid range for type", data->loc, "value", data->type, val)) __debugbreak();
}

// ---- Invalid builtin (__builtin_ctz/clz(0), __builtin_assume(false)) ----

static bool impl_invalid_builtin(InvalidBuiltinData* data)
{
    const char* msg;
    char buf[96];
    switch (data->kind) {
    case 0:  msg = "passing zero to __builtin_ctz(), which is undefined"; break;
    case 1:  msg = "passing zero to __builtin_clz(), which is undefined"; break;
    case 2:  msg = "__builtin_assume() evaluated to false";                break;
    default:
        sprintf_s(buf, sizeof(buf), "invalid use of builtin (kind=%u)", (unsigned)data->kind);
        msg = buf;
        break;
    }
    return ubsan_report(data, msg, data->loc);
}

__declspec(dllexport) void __ubsan_handle_invalid_builtin(InvalidBuiltinData* data)
{
    if (impl_invalid_builtin(data)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_invalid_builtin_abort(InvalidBuiltinData* data)
{
    if (impl_invalid_builtin(data)) __debugbreak();
}

// ---- Nonnull argument / return ----

static bool impl_nonnull_arg(NonNullArgData* data)
{
    if (isDuplicate(data)) return false;
    char buffer[2048];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: null pointer passed to nonnull argument ***\n    argument index: %d\n    at %s:%u:%u\n",
        data->argIndex,
        data->loc.filename, data->loc.line, data->loc.column);
    appendSourceContext(buffer, sizeof(buffer), data->loc.filename, data->loc.line);
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_nonnull_arg(NonNullArgData* data)
{
    if (impl_nonnull_arg(data)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_nonnull_arg_abort(NonNullArgData* data)
{
    if (impl_nonnull_arg(data)) __debugbreak();
}

// Dedup key uses loc (the return-statement SourceLocation*) -- a static const
// unique per return site, same dedup guarantee as using data*.
__declspec(dllexport) void __ubsan_handle_nonnull_return_v1(NonNullReturnData* data, SourceLocation* loc)
{
    if (ubsan_report(loc, "null returned from function declared to never return null", *loc)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_nonnull_return_v1_abort(NonNullReturnData* data, SourceLocation* loc)
{
    if (ubsan_report(loc, "null returned from function declared to never return null", *loc)) __debugbreak();
}

// ---- Pointer overflow ----

static bool impl_pointer_overflow(PointerOverflowData* data, ValueHandle base, ValueHandle result)
{
    if (isDuplicate(data)) return false;

    char baseDesc[128], resultDesc[128];
    describeMemoryRegion(baseDesc, sizeof(baseDesc), (uintptr_t)base);
    describeMemoryRegion(resultDesc, sizeof(resultDesc), (uintptr_t)result);

    char buffer[2048];
    if (base == 0 && result == 0) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: pointer overflow ***\n    applying zero offset to null pointer\n    at %s:%u:%u\n",
            data->loc.filename, data->loc.line, data->loc.column);
    } else if (base == 0) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: pointer overflow ***\n    applying non-zero offset to null pointer (result: 0x%p)\n    result memory: %s\n    at %s:%u:%u\n",
            (void*)result, resultDesc,
            data->loc.filename, data->loc.line, data->loc.column);
    } else {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: pointer overflow ***\n    pointer 0x%p with offset overflowed to 0x%p\n    base memory: %s\n    result memory: %s\n    at %s:%u:%u\n",
            (void*)base, (void*)result, baseDesc, resultDesc,
            data->loc.filename, data->loc.line, data->loc.column);
    }
    appendSourceContext(buffer, sizeof(buffer), data->loc.filename, data->loc.line);
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_pointer_overflow(PointerOverflowData* data, ValueHandle base, ValueHandle result)
{
    if (impl_pointer_overflow(data, base, result)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_pointer_overflow_abort(PointerOverflowData* data, ValueHandle base, ValueHandle result)
{
    if (impl_pointer_overflow(data, base, result)) __debugbreak();
}

// ---- Function type mismatch (indirect call through wrong-type pointer) ----

static bool impl_function_type_mismatch(FunctionTypeMismatchData* data, ValueHandle ptr)
{
    if (isDuplicate(data)) return false;
    char buffer[2048];
    sprintf_s(buffer, sizeof(buffer),
        "\n*** UBSAN: indirect function call type mismatch ***\n    call through pointer 0x%p to function of wrong type %s\n    at %s:%u:%u\n",
        (void*)ptr,
        data->type ? data->type->typeName : "<unknown>",
        data->loc.filename, data->loc.line, data->loc.column);
    appendSourceContext(buffer, sizeof(buffer), data->loc.filename, data->loc.line);
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_function_type_mismatch(FunctionTypeMismatchData* data, ValueHandle ptr)
{
    if (impl_function_type_mismatch(data, ptr)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_function_type_mismatch_abort(FunctionTypeMismatchData* data, ValueHandle ptr)
{
    if (impl_function_type_mismatch(data, ptr)) __debugbreak();
}

// ---- Alignment assumption ----

static bool impl_alignment_assumption(AlignmentAssumptionData* data, ValueHandle ptr, ValueHandle align, ValueHandle offset)
{
    if (isDuplicate(data)) return false;
    char buffer[2048];
    if (offset) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: alignment assumption violated ***\n    address 0x%p with offset %u does not meet alignment %u for type %s\n    at %s:%u:%u\n",
            (void*)ptr, (unsigned)offset, (unsigned)align,
            data->type ? data->type->typeName : "<unknown>",
            data->loc.filename, data->loc.line, data->loc.column);
    } else {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: alignment assumption violated ***\n    address 0x%p does not meet alignment %u for type %s\n    at %s:%u:%u\n",
            (void*)ptr, (unsigned)align,
            data->type ? data->type->typeName : "<unknown>",
            data->loc.filename, data->loc.line, data->loc.column);
    }
    appendSourceContext(buffer, sizeof(buffer), data->loc.filename, data->loc.line);
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_alignment_assumption(AlignmentAssumptionData* data, ValueHandle ptr, ValueHandle align, ValueHandle offset)
{
    if (impl_alignment_assumption(data, ptr, align, offset)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_alignment_assumption_abort(AlignmentAssumptionData* data, ValueHandle ptr, ValueHandle align, ValueHandle offset)
{
    if (impl_alignment_assumption(data, ptr, align, offset)) __debugbreak();
}

// ---- Implicit conversion (truncation, sign change) ----

static const char* getImplicitConversionKindName(unsigned char kind)
{
    static const char* names[] = {
        "integer truncation",              // 0 (legacy clang 7)
        "unsigned integer truncation",     // 1
        "signed integer truncation",       // 2
        "integer sign change",             // 3
        "signed truncation or sign change" // 4
    };
    return kind < sizeof(names)/sizeof(names[0]) ? names[kind] : "implicit conversion";
}

static bool impl_implicit_conversion(ImplicitConversionData* data, ValueHandle src, ValueHandle dst)
{
    if (isDuplicate(data)) return false;
    char srcStr[64], dstStr[64];
    formatValue(srcStr, sizeof(srcStr), data->fromType, src);
    formatValue(dstStr, sizeof(dstStr), data->toType,   dst);
    char buffer[2048];
    if (data->BitfieldBits) {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: implicit conversion (%s) ***\n    value %s (type %s, bitfield %u bits) changed to %s (type %s)\n    at %s:%u:%u\n",
            getImplicitConversionKindName(data->kind),
            srcStr, data->fromType ? data->fromType->typeName : "<unknown>", data->BitfieldBits,
            dstStr, data->toType   ? data->toType->typeName   : "<unknown>",
            data->loc.filename ? data->loc.filename : "<unknown>",
            data->loc.line, data->loc.column);
    } else {
        sprintf_s(buffer, sizeof(buffer),
            "\n*** UBSAN: implicit conversion (%s) ***\n    value %s (type %s) changed to %s (type %s)\n    at %s:%u:%u\n",
            getImplicitConversionKindName(data->kind),
            srcStr, data->fromType ? data->fromType->typeName : "<unknown>",
            dstStr, data->toType   ? data->toType->typeName   : "<unknown>",
            data->loc.filename ? data->loc.filename : "<unknown>",
            data->loc.line, data->loc.column);
    }
    appendSourceContext(buffer, sizeof(buffer), data->loc.filename ? data->loc.filename : "", data->loc.line);
    ubsan_output(buffer);
    return true;
}

__declspec(dllexport) void __ubsan_handle_implicit_conversion(ImplicitConversionData* data, ValueHandle src, ValueHandle dst)
{
    if (impl_implicit_conversion(data, src, dst)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_implicit_conversion_abort(ImplicitConversionData* data, ValueHandle src, ValueHandle dst)
{
    if (impl_implicit_conversion(data, src, dst)) __debugbreak();
}

// ---- Local out-of-bounds (-fsanitize=local-bounds, added Dec 2024) ----
// No data struct or source location -- the compiler inserts this as a trap
// at the point of the violation.  We can only report a generic message.

__declspec(dllexport) void __ubsan_handle_local_out_of_bounds()
{
    static volatile LONG s_reported = 0;
    if (InterlockedCompareExchange(&s_reported, 1, 0) != 0) return;
    ubsan_output("\n*** UBSAN: local array out of bounds (no source location available) ***\n");
    ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_local_out_of_bounds_abort()
{
    static volatile LONG s_reported = 0;
    if (InterlockedCompareExchange(&s_reported, 1, 0) != 0) return;
    ubsan_output("\n*** UBSAN: local array out of bounds (no source location available) ***\n");
    __debugbreak();
}

// ---- Nullability annotations (C _Nonnull; same layout as nonnull_*) ----
// These fire for _Nonnull-annotated pointers in headers compiled as C.
// Identical data structs, so we alias directly to the nonnull handlers.

__declspec(dllexport) void __ubsan_handle_nullability_arg(NonNullArgData* data)
{
    if (impl_nonnull_arg(data)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_nullability_arg_abort(NonNullArgData* data)
{
    if (impl_nonnull_arg(data)) __debugbreak();
}

__declspec(dllexport) void __ubsan_handle_nullability_return_v1(NonNullReturnData* data, SourceLocation* loc)
{
    if (ubsan_report(loc, "null returned from _Nonnull-annotated function", *loc)) ubsan_break();
}

__declspec(dllexport) void __ubsan_handle_nullability_return_v1_abort(NonNullReturnData* data, SourceLocation* loc)
{
    if (ubsan_report(loc, "null returned from _Nonnull-annotated function", *loc)) __debugbreak();
}

} // extern "C"


#pragma clang attribute pop

#endif // VPDEBUG
