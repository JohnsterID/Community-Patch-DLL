/*
 * asan_shadow_boot.dll
 *
 * Shadow-range pre-reservation shim for 32-bit AddressSanitizer on Windows.
 *
 * Problem
 * -------
 * CvGameCore_Expansion2.dll (VP mod, ASan-instrumented) loads late — only
 * after the user navigates to the mod menu.  By that time, D3D and GPU driver
 * initialisation have placed anonymous VirtualAlloc pools inside the ASan
 * shadow range [0x30000000, 0x35FFFFFF], so __asan_init() aborts.
 *
 * The game loads TWO DLLs with this name in sequence:
 *   1. Vanilla CvGameCore_Expansion2.dll (game dir) — no ASan.
 *   2. VP mod CvGameCore_Expansion2.dll (MODS dir)  — ASan-instrumented.
 *
 * Solution
 * --------
 * Phase A — DLL_PROCESS_ATTACH (injected by asan_launcher.exe before D3D):
 *   VirtualAlloc([SHADOW_BASE, SHADOW_END), MEM_RESERVE, PAGE_NOACCESS).
 *   GPU VirtualAlloc(NULL,..) calls cannot land in that range.
 *
 * Phase B — Release the reservation BEFORE __asan_init() claims it:
 *   We must free the reservation after the vanilla DLL loads (so GPU can't
 *   reclaim it) but before the ASan runtime's DllMain runs on the VP mod load.
 *
 *   TWO triggers are installed in DllMain, whichever fires first wins:
 *
 *   B1 (PRIMARY): LdrRegisterDllNotification
 *     Watches for any DLL whose base name starts with "CvGameCore_Expansion2".
 *     Fires AFTER that DLL's DllMain.  For the vanilla load this is safe —
 *     it fires well before the VP mod load begins.
 *     Does NOT depend on IAT contents or LoadLibraryA vs W.
 *
 *   B2 (SECONDARY): IAT hooks on LoadLibraryA AND LoadLibraryW in the EXE.
 *     Intercepts LoadLibrary calls, checks the filename prefix, then frees.
 *     Fires earlier (before DllMain) but only works if those functions appear
 *     in the EXE's IAT (not via GetProcAddress).
 *
 * All events are logged to asan_shadow_boot_debug.log in the game directory
 * so the exact trigger path can be verified.
 *
 * Deployment
 * ----------
 * Use asan_launcher.exe (built by build_vp_clang.py --sanitizer asan):
 *   asan_launcher.exe CivilizationV.exe
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

#define SHADOW_BASE ((LPVOID)0x2FFF0000)
#define SHADOW_SIZE ((SIZE_T)(0x50000000 - 0x2FFF0000))  /* 512 MB + 64 KB */

#define CVGAME_PREFIX    "CvGameCore_Expansion2"
#define CVGAME_PREFIX_W L"CvGameCore_Expansion2"
#define CVGAME_PREFIX_LEN 21


/* ------------------------------------------------------------------ */
/*  File log (written to game CWD as asan_shadow_boot_debug.log)      */
/* ------------------------------------------------------------------ */

static FILE *g_log;

static void
blog(const char *fmt, ...)
{
    if (!g_log) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(g_log, fmt, ap); fputc('\n', g_log);
    fflush(g_log);
    va_end(ap);
}

static void
log_open(void)
{
    g_log = fopen("asan_shadow_boot_debug.log", "w");
    /* If CWD is not the game dir, try the DLL's own directory */
    if (!g_log) {
        char dir[MAX_PATH]; DWORD n;
        n = GetModuleFileNameA(GetModuleHandleA("asan_shadow_boot.dll"), dir, MAX_PATH);
        while (n > 0 && dir[n-1] != '\\') --n;
        if (n) { dir[n] = '\0'; strcat(dir, "asan_shadow_boot_debug.log"); }
        g_log = fopen(dir, "w");
    }
}


/* ------------------------------------------------------------------ */
/*  One-shot shadow release                                            */
/* ------------------------------------------------------------------ */

static volatile LONG g_released;

static void
release_shadow(const char *trigger)
{
    if (InterlockedCompareExchange(&g_released, 1, 0) != 0) return;
    BOOL ok = VirtualFree(SHADOW_BASE, 0, MEM_RELEASE);
    blog("[+] release_shadow via <%s>: VirtualFree(%p) = %s",
         trigger, SHADOW_BASE, ok ? "OK" : "FAILED (was not reserved by us)");
}


/* ------------------------------------------------------------------ */
/*  IAT slot scanner (same base as before)                             */
/* ------------------------------------------------------------------ */

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

        IMAGE_THUNK_DATA *orig =
            (IMAGE_THUNK_DATA *)(base + imp->OriginalFirstThunk);
        IMAGE_THUNK_DATA *thunk =
            (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);

        for (; orig->u1.AddressOfData; ++orig, ++thunk) {
            if (IMAGE_SNAP_BY_ORDINAL(orig->u1.Ordinal)) continue;
            IMAGE_IMPORT_BY_NAME *ibn =
                (IMAGE_IMPORT_BY_NAME *)(base + orig->u1.AddressOfData);
            if (lstrcmpiA((const char *)ibn->Name, func_name) == 0)
                return (FARPROC *)&thunk->u1.Function;
        }
        break;
    }
    return NULL;
}


/* ------------------------------------------------------------------ */
/*  IAT hooks (B2 — secondary trigger)                                 */
/* ------------------------------------------------------------------ */

static FARPROC *g_slot_a;   /* LoadLibraryA IAT slot in EXE  */
static FARPROC *g_slot_w;   /* LoadLibraryW IAT slot in EXE  */
static FARPROC  g_real_a;
static FARPROC  g_real_w;

/* Restore one IAT slot */
static void
restore_slot(FARPROC *slot, FARPROC real)
{
    if (!slot) return;
    DWORD old;
    VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old);
    *slot = real;
    VirtualProtect(slot, sizeof *slot, old, &old);
}

/* Check if an ANSI DLL name starts with the Civ game-core prefix */
static int
is_cvgame_a(LPCSTR name)
{
    if (!name) return 0;
    /* walk to last separator */
    for (LPCSTR p = name; *p; ++p)
        if (*p == '\\' || *p == '/') name = p + 1;
    int len = lstrlenA(name);
    return len >= CVGAME_PREFIX_LEN &&
           CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE,
                          name, CVGAME_PREFIX_LEN,
                          CVGAME_PREFIX, CVGAME_PREFIX_LEN) == CSTR_EQUAL;
}

/* Check if a wide DLL name starts with the Civ game-core prefix */
static int
is_cvgame_w(LPCWSTR name)
{
    if (!name) return 0;
    for (LPCWSTR p = name; *p; ++p)
        if (*p == L'\\' || *p == L'/') name = p + 1;
    int len = lstrlenW(name);
    return len >= CVGAME_PREFIX_LEN &&
           CompareStringOrdinal(name, CVGAME_PREFIX_LEN,
                                CVGAME_PREFIX_W, CVGAME_PREFIX_LEN,
                                TRUE) == CSTR_EQUAL;
}

static HMODULE WINAPI
hook_LoadLibraryA(LPCSTR name)
{
    blog("[B2-A] LoadLibraryA(\"%s\")", name ? name : "(null)");
    if (is_cvgame_a(name)) {
        restore_slot(g_slot_a, g_real_a); g_slot_a = NULL;
        restore_slot(g_slot_w, g_real_w); g_slot_w = NULL;
        release_shadow("IAT-LoadLibraryA");
    }
    return ((HMODULE(WINAPI*)(LPCSTR))g_real_a)(name);
}

static HMODULE WINAPI
hook_LoadLibraryW(LPCWSTR name)
{
    blog("[B2-W] LoadLibraryW(\"%ls\")", name ? name : L"(null)");
    if (is_cvgame_w(name)) {
        restore_slot(g_slot_a, g_real_a); g_slot_a = NULL;
        restore_slot(g_slot_w, g_real_w); g_slot_w = NULL;
        release_shadow("IAT-LoadLibraryW");
    }
    return ((HMODULE(WINAPI*)(LPCWSTR))g_real_w)(name);
}

static void
install_iat_hooks(void)
{
    DWORD old;

    g_slot_a = find_iat_slot("KERNEL32.DLL", "LoadLibraryA");
    if (g_slot_a) {
        VirtualProtect(g_slot_a, sizeof *g_slot_a, PAGE_READWRITE, &old);
        g_real_a = *g_slot_a;
        *g_slot_a = (FARPROC)hook_LoadLibraryA;
        VirtualProtect(g_slot_a, sizeof *g_slot_a, old, &old);
        blog("[B2] IAT hook installed: LoadLibraryA @ %p", g_slot_a);
    } else {
        blog("[B2] LoadLibraryA not in EXE IAT — hook skipped");
    }

    g_slot_w = find_iat_slot("KERNEL32.DLL", "LoadLibraryW");
    if (g_slot_w) {
        VirtualProtect(g_slot_w, sizeof *g_slot_w, PAGE_READWRITE, &old);
        g_real_w = *g_slot_w;
        *g_slot_w = (FARPROC)hook_LoadLibraryW;
        VirtualProtect(g_slot_w, sizeof *g_slot_w, old, &old);
        blog("[B2] IAT hook installed: LoadLibraryW @ %p", g_slot_w);
    } else {
        blog("[B2] LoadLibraryW not in EXE IAT — hook skipped");
    }

    if (!g_slot_a && !g_slot_w)
        blog("[B2] WARNING: neither LoadLibraryA nor LoadLibraryW found in EXE IAT");
}


/* ------------------------------------------------------------------ */
/*  LdrRegisterDllNotification (B1 — primary trigger)                  */
/* ------------------------------------------------------------------ */

/* Minimal definitions for undocumented ntdll API (stable since Vista) */
typedef struct {
    ULONG    Flags;
    PUNICODE_STRING FullDllName;
    PUNICODE_STRING BaseDllName;
    PVOID    DllBase;
    ULONG    SizeOfImage;
} MY_LDR_DATA;

typedef VOID (CALLBACK *MY_NOTIFY_FN)(ULONG reason, MY_LDR_DATA *data, PVOID ctx);
typedef NTSTATUS (NTAPI *PFN_REGISTER)(ULONG flags, MY_NOTIFY_FN fn,
                                       PVOID ctx, PVOID *cookie);

#define LDR_DLL_NOTIFICATION_REASON_LOADED   1
#define LDR_DLL_NOTIFICATION_REASON_UNLOADED 2

static PVOID g_ldr_cookie;

static VOID CALLBACK
ldr_notify(ULONG reason, MY_LDR_DATA *data, PVOID ctx)
{
    (void)ctx;
    if (reason != LDR_DLL_NOTIFICATION_REASON_LOADED) return;
    if (!data || !data->BaseDllName || !data->BaseDllName->Buffer) return;

    PUNICODE_STRING n = data->BaseDllName;
    int len_chars = n->Length / sizeof(WCHAR);

    blog("[B1] DLL loaded: %.*ls  base=%p size=%lu",
         len_chars, n->Buffer, data->DllBase, data->SizeOfImage);

    if (len_chars >= CVGAME_PREFIX_LEN &&
        CompareStringOrdinal(n->Buffer, CVGAME_PREFIX_LEN,
                             CVGAME_PREFIX_W, CVGAME_PREFIX_LEN,
                             TRUE) == CSTR_EQUAL)
    {
        /* Remove IAT hooks too so there's no double-free attempt */
        restore_slot(g_slot_a, g_real_a); g_slot_a = NULL;
        restore_slot(g_slot_w, g_real_w); g_slot_w = NULL;
        release_shadow("LdrDllNotification");
    }
}

static void
register_ldr_notification(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    PFN_REGISTER fn = (PFN_REGISTER)GetProcAddress(ntdll, "LdrRegisterDllNotification");
    if (!fn) {
        blog("[B1] LdrRegisterDllNotification not found in ntdll — skipped");
        return;
    }
    NTSTATUS st = fn(0, ldr_notify, NULL, &g_ldr_cookie);
    blog("[B1] LdrRegisterDllNotification: status=0x%08lX  cookie=%p",
         (unsigned long)st, g_ldr_cookie);
}


/* ------------------------------------------------------------------ */
/*  DllMain                                                            */
/* ------------------------------------------------------------------ */

BOOL WINAPI
DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;

    DisableThreadLibraryCalls(hInst);

    /* Only act inside CivilizationV.exe */
    if (!GetModuleHandleW(L"CivilizationV.exe")) return TRUE;

    log_open();
    blog("[A] DllMain DLL_PROCESS_ATTACH  hInst=%p", (void*)hInst);

    /* Phase A: reserve shadow range before D3D/GPU init */
    LPVOID res = VirtualAlloc(SHADOW_BASE, SHADOW_SIZE,
                              MEM_RESERVE, PAGE_NOACCESS);
    blog("[A] VirtualAlloc(SHADOW_BASE, 512MB, MEM_RESERVE): %s  addr=%p",
         res ? "OK" : "FAILED (range already occupied)", res);

    /* Phase B1: LdrRegisterDllNotification — primary release trigger */
    register_ldr_notification();

    /* Phase B2: IAT hooks on LoadLibraryA + LoadLibraryW — secondary */
    install_iat_hooks();

    blog("[A] Initialisation complete.  Waiting for CvGameCore_Expansion2 load...");
    return TRUE;
}
