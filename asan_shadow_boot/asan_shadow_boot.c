/*
 * asan_shadow_boot.dll
 *
 * Shadow-range pre-reservation shim for 32-bit AddressSanitizer on Windows.
 *
 * Problem
 * -------
 * clang_rt.asan_dynamic-i386.dll is loaded late (as a dependency of the VP
 * mod DLL) and __asan_init() calls VirtualAlloc to claim the shadow range
 * [0x30000000, 0x35FFFFFF].  Two failure modes observed across sessions:
 *
 *   Run 17468: shadow reserved at injection, reservation never freed
 *              -> __asan_init VirtualAlloc hit our own reservation -> abort
 *   Run 23856: reservation freed on vanilla DLL load, ASLR then placed
 *              the ASan DLL IMAGE at 0x2FD70000 (spanning into shadow)
 *              -> __asan_init could not map shadow -> abort
 *
 * Both failures stem from the same root cause: the reservation was released
 * at the WRONG time.  The correct release point is *inside* the ASan DLL's
 * DllMain, at the exact VirtualAlloc call __asan_init() makes for the shadow.
 *
 * Solution
 * --------
 * Phase A (DLL_PROCESS_ATTACH, before D3D/GPU init):
 *   VirtualAlloc([SHADOW_BASE, SHADOW_END), MEM_RESERVE, PAGE_NOACCESS).
 *   GPU anonymous pools cannot claim the shadow range.
 *   ASLR cannot place the ASan DLL image in the shadow range.
 *
 * Phase B (via LdrRegisterDllNotification, fires BEFORE DllMain on Win10/11):
 *   When "clang_rt.asan_dynamic" is detected loading:
 *     - Shadow is still reserved -> ASan DLL placed safely outside shadow
 *     - Hook VirtualAlloc in the ASan DLL's own IAT
 *   When hooked VirtualAlloc is called with a shadow-range address:
 *     - VirtualFree our reservation (exactly when __asan_init needs the range)
 *     - Restore real VirtualAlloc in IAT (one-shot)
 *     - Forward the call -> __asan_init claims the shadow -> SUCCESS
 *
 * Diagnostic:
 *   Every event is logged to asan_shadow_boot_debug.log in the game directory
 *   (CWD when DllMain runs, typically the game install directory).
 *   IAT hooks on LoadLibraryA + W in the EXE are retained as diagnostic logging
 *   only — they no longer release the shadow.
 *
 * Deployment:
 *   asan_launcher.exe (built by build_vp_clang.py --sanitizer asan) creates
 *   CivilizationV.exe as a suspended process and injects this DLL before the
 *   main thread starts.
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

/* DLL name prefixes to watch for */
#define CVGAME_PREFIX    "CvGameCore_Expansion2"
#define CVGAME_PREFIX_W L"CvGameCore_Expansion2"
#define CVGAME_PFX_LEN  21

#define ASAN_RT_PREFIX    "clang_rt.asan_dynamic"
#define ASAN_RT_PREFIX_W L"clang_rt.asan_dynamic"
#define ASAN_RT_PFX_LEN  21


/* ------------------------------------------------------------------ */
/*  File log (game CWD -> asan_shadow_boot_debug.log)                  */
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
    /* Try CWD first (== game install dir when launched by asan_launcher.exe) */
    g_log = fopen("asan_shadow_boot_debug.log", "w");
    if (!g_log) {
        /* Fallback: next to the DLL itself */
        char dir[MAX_PATH];
        DWORD n = GetModuleFileNameA(
            GetModuleHandleA("asan_shadow_boot.dll"), dir, MAX_PATH);
        while (n > 0 && dir[n-1] != '\\') --n;
        if (n) { dir[n] = '\0'; strcat(dir, "asan_shadow_boot_debug.log"); }
        g_log = fopen(dir, "w");
    }
}


/* ------------------------------------------------------------------ */
/*  Shadow reservation release (one-shot)                              */
/* ------------------------------------------------------------------ */

static volatile LONG g_released;

static void
release_shadow(const char *trigger)
{
    if (InterlockedCompareExchange(&g_released, 1, 0) != 0) {
        blog("    release_shadow: already released (called from <%s>)", trigger);
        return;
    }
    BOOL ok = VirtualFree(SHADOW_BASE, 0, MEM_RELEASE);
    blog("[+] release_shadow via <%s>: VirtualFree(%p) = %s",
         trigger, SHADOW_BASE, ok ? "OK" : "FAILED");
}


/* ------------------------------------------------------------------ */
/*  IAT scanner — can target any PE base (EXE or a loaded DLL)        */
/* ------------------------------------------------------------------ */

static FARPROC *
find_iat_slot_in(PVOID module_base, const char *dll_name, const char *func_name)
{
    BYTE *base = (BYTE *)module_base;
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

/* Convenience wrapper: search the EXE (module 0) */
static FARPROC *
find_iat_slot(const char *dll_name, const char *func_name)
{
    return find_iat_slot_in(GetModuleHandleW(NULL), dll_name, func_name);
}

static void
patch_iat(FARPROC *slot, FARPROC new_fn, FARPROC *saved)
{
    DWORD old;
    VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old);
    if (saved) *saved = *slot;
    *slot = new_fn;
    VirtualProtect(slot, sizeof *slot, old, &old);
}


/* ------------------------------------------------------------------ */
/*  VirtualAlloc hook — planted in the ASan DLL's own IAT             */
/*                                                                     */
/*  Fires when __asan_init() calls VirtualAlloc for the shadow range.  */
/*  Releases our reservation at the last possible moment so the call   */
/*  succeeds.                                                           */
/* ------------------------------------------------------------------ */

static FARPROC *g_va_slot;    /* VirtualAlloc slot in ASan DLL's IAT */
static FARPROC  g_real_va;

static LPVOID WINAPI
hooked_VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize,
                    DWORD  flAllocationType, DWORD flProtect)
{
    blog("[B-VA] VirtualAlloc(%p, %Iu, 0x%08lX, 0x%08lX)",
         lpAddress, dwSize, (unsigned long)flAllocationType,
         (unsigned long)flProtect);

    /* Detect __asan_init's shadow claim: MEM_RESERVE in the shadow range */
    if ((flAllocationType & MEM_RESERVE) &&
        (DWORD)(DWORD_PTR)lpAddress >= (DWORD)(DWORD_PTR)SHADOW_BASE &&
        (DWORD)(DWORD_PTR)lpAddress < 0x50000000u)
    {
        blog("[B-VA] Shadow-range VirtualAlloc detected — releasing reservation");

        /* Restore real VirtualAlloc FIRST so recursive calls are clean */
        FARPROC *slot = g_va_slot;
        if (slot) {
            patch_iat(slot, g_real_va, NULL);
            g_va_slot = NULL;
        }
        /* Release our shadow reservation so the real VirtualAlloc succeeds */
        release_shadow("VirtualAlloc-hook");
    }

    return ((LPVOID(WINAPI*)(LPVOID,SIZE_T,DWORD,DWORD))g_real_va)
               (lpAddress, dwSize, flAllocationType, flProtect);
}


/* ------------------------------------------------------------------ */
/*  LoadLibrary IAT hooks on EXE — diagnostic only (no VirtualFree)   */
/* ------------------------------------------------------------------ */

static FARPROC *g_slot_a;
static FARPROC *g_slot_w;
static FARPROC  g_real_a;
static FARPROC  g_real_w;

static int
name_starts_a(LPCSTR name, const char *prefix, int prefix_len)
{
    if (!name) return 0;
    for (LPCSTR p = name; *p; ++p)
        if (*p == '\\' || *p == '/') name = p + 1;
    return lstrlenA(name) >= prefix_len &&
           CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE,
                          name, prefix_len, prefix, prefix_len) == CSTR_EQUAL;
}

static HMODULE WINAPI
hook_LoadLibraryA(LPCSTR name)
{
    blog("[diag-A] LoadLibraryA(\"%s\")", name ? name : "(null)");
    if (name && name_starts_a(name, CVGAME_PREFIX, CVGAME_PFX_LEN))
        blog("         -> CvGameCore matched (shadow NOT released here)");
    return ((HMODULE(WINAPI*)(LPCSTR))g_real_a)(name);
}

static HMODULE WINAPI
hook_LoadLibraryW(LPCWSTR name)
{
    if (name) {
        char narrow[MAX_PATH]; WideCharToMultiByte(CP_UTF8,0,name,-1,narrow,MAX_PATH,NULL,NULL);
        blog("[diag-W] LoadLibraryW(\"%s\")", narrow);
    }
    return ((HMODULE(WINAPI*)(LPCWSTR))g_real_w)(name);
}

static void
install_loadlibrary_diag(void)
{
    g_slot_a = find_iat_slot("KERNEL32.DLL", "LoadLibraryA");
    if (g_slot_a) { patch_iat(g_slot_a, (FARPROC)hook_LoadLibraryA, &g_real_a);
                    blog("[diag] LoadLibraryA IAT hooked @ %p", g_slot_a); }
    else           blog("[diag] LoadLibraryA not in EXE IAT");

    g_slot_w = find_iat_slot("KERNEL32.DLL", "LoadLibraryW");
    if (g_slot_w) { patch_iat(g_slot_w, (FARPROC)hook_LoadLibraryW, &g_real_w);
                    blog("[diag] LoadLibraryW IAT hooked @ %p", g_slot_w); }
    else           blog("[diag] LoadLibraryW not in EXE IAT");
}


/* ------------------------------------------------------------------ */
/*  LdrRegisterDllNotification                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    ULONG           Flags;
    PUNICODE_STRING FullDllName;
    PUNICODE_STRING BaseDllName;
    PVOID           DllBase;
    ULONG           SizeOfImage;
} MY_LDR_DATA;

typedef VOID   (CALLBACK *MY_NOTIFY_FN)(ULONG, MY_LDR_DATA *, PVOID);
typedef NTSTATUS (NTAPI *PFN_REGISTER) (ULONG, MY_NOTIFY_FN, PVOID, PVOID *);

#define LDR_DLL_NOTIFICATION_REASON_LOADED   1
#define LDR_DLL_NOTIFICATION_REASON_UNLOADED 2

static PVOID g_ldr_cookie;

static int
us_starts_w(PUNICODE_STRING us, const WCHAR *prefix, int prefix_len)
{
    if (!us || !us->Buffer) return 0;
    int len = us->Length / (int)sizeof(WCHAR);
    return len >= prefix_len &&
           CompareStringOrdinal(us->Buffer, prefix_len,
                                prefix, prefix_len, TRUE) == CSTR_EQUAL;
}

static VOID CALLBACK
ldr_notify(ULONG reason, MY_LDR_DATA *data, PVOID ctx)
{
    (void)ctx;
    if (reason != LDR_DLL_NOTIFICATION_REASON_LOADED) return;
    if (!data || !data->BaseDllName || !data->BaseDllName->Buffer) return;

    PUNICODE_STRING n = data->BaseDllName;
    int len = n->Length / (int)sizeof(WCHAR);
    blog("[B1] DLL loaded: %.*ls  base=%p  size=0x%lX",
         len, n->Buffer, data->DllBase, data->SizeOfImage);

    /* Diagnostic: log vanilla/mod CvGameCore loads (shadow NOT released here) */
    if (us_starts_w(n, CVGAME_PREFIX_W, CVGAME_PFX_LEN)) {
        blog("     -> CvGameCore_Expansion2 detected (shadow kept reserved)");
        return;
    }

    /* Primary trigger: clang_rt.asan_dynamic — fires BEFORE DllMain on Win10/11
     * At this point the DLL is mapped but __asan_init() has not run yet.
     * Shadow is still reserved -> DLL image could not be placed in shadow.
     * Hook VirtualAlloc in ASan DLL's IAT so we can release the reservation
     * at the exact moment __asan_init() calls VirtualAlloc for the shadow. */
    if (us_starts_w(n, ASAN_RT_PREFIX_W, ASAN_RT_PFX_LEN)) {
        blog("     -> ASan runtime detected — hooking VirtualAlloc in its IAT");

        FARPROC *slot = find_iat_slot_in(data->DllBase, "KERNEL32.DLL", "VirtualAlloc");
        if (slot) {
            patch_iat(slot, (FARPROC)hooked_VirtualAlloc, &g_real_va);
            g_va_slot = slot;
            blog("     VirtualAlloc IAT slot @ %p  real=%p", slot, g_real_va);
        } else {
            blog("     WARNING: VirtualAlloc not found in ASan DLL IAT");
            blog("     Falling back: releasing shadow now (may still conflict)");
            release_shadow("LdrDllNotification-fallback");
        }
        return;
    }
}

static void
register_ldr_notification(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    PFN_REGISTER fn = (PFN_REGISTER)GetProcAddress(ntdll, "LdrRegisterDllNotification");
    if (!fn) { blog("[B1] LdrRegisterDllNotification not in ntdll"); return; }
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
    if (!GetModuleHandleW(L"CivilizationV.exe")) return TRUE;

    log_open();
    blog("[A] DllMain DLL_PROCESS_ATTACH  hInst=%p", (void*)hInst);

    /* Phase A: reserve shadow range before D3D/GPU init */
    LPVOID res = VirtualAlloc(SHADOW_BASE, SHADOW_SIZE, MEM_RESERVE, PAGE_NOACCESS);
    blog("[A] VirtualAlloc(SHADOW_BASE, 512MB, MEM_RESERVE): %s  addr=%p",
         res ? "OK" : "FAILED (range already occupied)", res);

    /* Phase B primary: LdrDllNotification -> VirtualAlloc hook in ASan DLL IAT */
    register_ldr_notification();

    /* Diagnostic: log all LoadLibraryA/W calls from EXE (no release here) */
    install_loadlibrary_diag();

    blog("[A] Init complete. Shadow stays reserved until ASan DLL's VirtualAlloc.");
    return TRUE;
}
