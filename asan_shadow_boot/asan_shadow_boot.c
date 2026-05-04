/*
 * asan_shadow_boot.dll
 *
 * Shadow-range pre-reservation shim for 32-bit AddressSanitizer on Windows.
 *
 * Problem
 * -------
 * CivilizationV.exe loads its mod DLL (CvGameCore_Expansion2.dll) late,
 * only after the user navigates to the mod menu.  By that point, D3D and
 * GPU driver initialisation (Intel igd9trinity32, AMD AMDXN32) have already
 * placed anonymous VirtualAlloc pools in the 680 MB gap between the last
 * Miles audio module (0x26F17000) and the first Windows audio DLL image
 * (0x51790000).  One or more of those pools lands inside the ASan shadow
 * range [0x30000000, 0x35FFFFFF], so when clang_rt.asan_dynamic-i386.dll
 * loads as a dependency of the mod DLL, __asan_init() calls VirtualAlloc
 * for the shadow and gets NULL, then aborts: "Shadow memory range interleaves
 * with an existing memory mapping."
 *
 * Solution - two phases, one DLL
 * --------------------------------
 * Phase A (DLL_PROCESS_ATTACH, very early - before D3D/GPU init):
 *   Reserve [SHADOW_BASE, SHADOW_BASE + SHADOW_SIZE) with PAGE_NOACCESS.
 *   Subsequent GPU VirtualAlloc(NULL, ...) calls cannot land in that range
 *   because the range is already committed to this process.
 *
 * Phase B (IAT hook on LoadLibraryW, just before ASan claims the shadow):
 *   Civ5 loads TWO DLLs named CvGameCore_Expansion2.dll in sequence:
 *     1. The vanilla game DLL (game directory) — loaded at EXE startup,
 *        no ASan instrumentation, no shadow conflict.
 *     2. The VP mod DLL (MODS directory) — loaded after the user activates
 *        the mod via the in-game mod menu, fully ASan-instrumented.
 *   The hook fires on load #1 (vanilla DLL).  It releases the shadow
 *   reservation and unhooks itself.  By the time load #2 (VP mod DLL)
 *   happens, the shadow range is free: GPU drivers have already settled
 *   their allocations elsewhere (blocked by Phase A).  __asan_init() in
 *   clang_rt.asan_dynamic-i386.dll then VirtualAllocs the shadow and
 *   succeeds.  The Windows loader lock is held throughout LoadLibraryW,
 *   so no thread can sneak into the freed range before ASan claims it.
 *
 * Deployment
 * ----------
 * Preferred: use asan_launcher.exe (built alongside this DLL).  The
 * launcher creates CivilizationV.exe as a suspended process, injects this
 * DLL via CreateRemoteThread, then resumes — no registry changes needed.
 *
 *   asan_launcher.exe [path\to\CivilizationV.exe]
 *
 * Alternative (requires elevated prompt, affects all GUI processes):
 *   reg add "HKLM\SOFTWARE\Wow6432Node\Microsoft\Windows NT\CurrentVersion\Windows" ^
 *       /v AppInit_DLLs /t REG_SZ /d "<full path to asan_shadow_boot.dll>" /f
 *   reg add "HKLM\SOFTWARE\Wow6432Node\Microsoft\Windows NT\CurrentVersion\Windows" ^
 *       /v LoadAppInit_DLLs /t REG_DWORD /d 1 /f
 *   reg add "HKLM\SOFTWARE\Wow6432Node\Microsoft\Windows NT\CurrentVersion\Windows" ^
 *       /v RequireSignedAppInit_DLLs /t REG_DWORD /d 0 /f
 *
 * Notes
 * -----
 * - The DLL checks GetModuleHandleW("CivilizationV.exe") and returns
 *   immediately in all other processes (safe with AppInit_DLLs).
 * - build_vp_clang.py --sanitizer asan builds this DLL and asan_launcher.exe
 *   automatically and prints usage instructions.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/*
 * Full range that ASan probes for conflicts on init (from the abort message):
 *   "shadow was supposed to be located in the [0x2fff0000-0x4fffffff] range"
 * We reserve this exact extent so that GPU pool allocations cannot land here.
 */
#define SHADOW_BASE ((LPVOID)0x2FFF0000)
#define SHADOW_SIZE ((SIZE_T)(0x50000000 - 0x2FFF0000))  /* 512 MB + 64 KB */

/* State shared between DllMain and the hook */
static FARPROC  *volatile g_iat_slot;          /* pointer into EXE's IAT     */
static FARPROC            g_real_LoadLibraryW; /* saved original function ptr */
static LPVOID             g_reservation;       /* our VirtualAlloc handle     */
static volatile LONG      g_released;          /* one-shot flag               */


/* ------------------------------------------------------------------
 * find_iat_slot
 * Walk the PE import directory of the main EXE (GetModuleHandleW(NULL))
 * and return a pointer to the IAT entry for dll_name!func_name, or NULL.
 * ------------------------------------------------------------------ */
static FARPROC *
find_iat_slot(const char *dll_name, const char *func_name)
{
    BYTE *base = (BYTE *)GetModuleHandleW(NULL);
    if (!base) return NULL;

    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;

    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir =
        &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir->VirtualAddress || !dir->Size) return NULL;

    IMAGE_IMPORT_DESCRIPTOR *imp =
        (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress);

    for (; imp->Name; ++imp) {
        if (lstrcmpiA((const char *)(base + imp->Name), dll_name) != 0)
            continue;
        if (!imp->OriginalFirstThunk || !imp->FirstThunk)
            continue;

        IMAGE_THUNK_DATA *orig  =
            (IMAGE_THUNK_DATA *)(base + imp->OriginalFirstThunk);
        IMAGE_THUNK_DATA *thunk =
            (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);

        for (; orig->u1.AddressOfData; ++orig, ++thunk) {
            if (IMAGE_SNAP_BY_ORDINAL(orig->u1.Ordinal))
                continue;
            IMAGE_IMPORT_BY_NAME *ibn =
                (IMAGE_IMPORT_BY_NAME *)(base + orig->u1.AddressOfData);
            if (lstrcmpiA((const char *)ibn->Name, func_name) == 0)
                return (FARPROC *)&thunk->u1.Function;
        }
        break; /* found the right DLL, no need to keep looking */
    }
    return NULL;
}


/* ------------------------------------------------------------------
 * Shadow_LoadLibraryW  (the IAT hook)
 *
 * Called for every LoadLibraryW invocation that goes through the EXE's IAT.
 * When it detects the CvGameCore_Expansion2 load, it:
 *   1. Restores the original IAT entry (one-shot, unhooks itself).
 *   2. Releases the shadow reservation so __asan_init() gets a clean range.
 *   3. Falls through to the real LoadLibraryW.
 * For every other DLL name the function is transparent.
 * ------------------------------------------------------------------ */
static HMODULE WINAPI
Shadow_LoadLibraryW(LPCWSTR lpLibFileName)
{
    if (lpLibFileName && !InterlockedCompareExchange(&g_released, 1, 0)) {
        /* Walk to the last path separator to get the bare file name */
        const WCHAR *base = lpLibFileName;
        for (const WCHAR *p = lpLibFileName; *p; ++p)
            if (*p == L'\\' || *p == L'/')
                base = p + 1;

        /*
         * Check for the CvGameCore_Expansion2 prefix (21 characters),
         * case-insensitive, using Win32 only (no CRT dependency).
         */
        int len = lstrlenW(base);
        BOOL match = (len >= 21) &&
            (CompareStringOrdinal(base, 21,
                                  L"CvGameCore_Expansion2", 21,
                                  TRUE) == CSTR_EQUAL);

        if (match) {
            /* Restore IAT before the real call so re-entrant loads are clean */
            FARPROC *slot = g_iat_slot;
            if (slot) {
                DWORD old_prot;
                VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old_prot);
                *slot = g_real_LoadLibraryW;
                VirtualProtect(slot, sizeof *slot, old_prot, &old_prot);
                g_iat_slot = NULL;
            }
            /*
             * Release the shadow reservation.  Always free SHADOW_BASE
             * directly so this works whether the reservation was made by
             * Phase A (DllMain) or by an external launcher via VirtualAllocEx.
             * The loader lock is held for the duration of LoadLibraryW, so no
             * GPU thread can reclaim the range in the window between our
             * VirtualFree and ASan's VirtualAlloc inside __asan_init().
             *
             * Note: this fires on the FIRST CvGameCore_Expansion2 load
             * (the vanilla game DLL, no ASan).  That clears the range so
             * the subsequent VP mod DLL load can map the shadow successfully.
             */
            VirtualFree(SHADOW_BASE, 0, MEM_RELEASE);
            g_reservation = NULL;
        } else {
            /* Wrong DLL - allow the hook to fire again on the next call */
            InterlockedExchange(&g_released, 0);
        }
    }

    return ((HMODULE(WINAPI *)(LPCWSTR))g_real_LoadLibraryW)(lpLibFileName);
}


/* ------------------------------------------------------------------
 * DllMain
 * ------------------------------------------------------------------ */
BOOL WINAPI
DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);

        /*
         * Guard: only install the hook in CivilizationV.exe.
         * AppInit_DLLs injects into every GUI process; returning TRUE here
         * with no side-effects is safe and harmless in all other processes.
         */
        if (!GetModuleHandleW(L"CivilizationV.exe"))
            return TRUE;

        /*
         * Phase A: reserve the ASan shadow range before D3D / GPU driver
         * initialisation can allocate into it.
         *
         * If the reservation fails (range already occupied) we continue
         * anyway: the IAT hook still fires, and if the occupier has freed
         * the range by the time the mod loads, ASan may still succeed.
         */
        g_reservation = VirtualAlloc(SHADOW_BASE, SHADOW_SIZE,
                                     MEM_RESERVE, PAGE_NOACCESS);

        /*
         * Phase B: IAT-hook LoadLibraryW in the EXE so we can release the
         * reservation the instant the game tries to load CvGameCore_Expansion2.
         */
        FARPROC *slot = find_iat_slot("KERNEL32.DLL", "LoadLibraryW");
        if (slot) {
            DWORD old_prot;
            VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old_prot);
            g_real_LoadLibraryW = *slot;
            *slot = (FARPROC)Shadow_LoadLibraryW;
            VirtualProtect(slot, sizeof *slot, old_prot, &old_prot);
            g_iat_slot = slot;
        }
        /* If the IAT slot isn't found (unlikely) the reservation is still
         * useful: it blocks GPU drivers from the shadow range.  ASan will
         * fail as before, but no worse than without this DLL. */
    }
    return TRUE;
}
