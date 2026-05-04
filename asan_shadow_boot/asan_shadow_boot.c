/*
 * asan_shadow_boot.dll
 *
 * Shadow-range pre-reservation shim for 32-bit AddressSanitizer on Windows.
 *
 * Problem
 * -------
 * clang_rt.asan_dynamic-i386.dll is loaded late (as a dependency of the VP
 * mod DLL).  __asan_init() must claim the shadow range [0x2fff0000-0x4fffffff]
 * but by that point GPU drivers (amdxc32, igc32, etc.) have consumed large
 * anonymous pools that can fall inside that range.  If any mapping overlaps
 * the shadow range, __asan_init aborts with:
 *   "Shadow memory range interleaves with an existing memory mapping."
 *
 * Key finding (PID 11988 / asan_game.log):
 *   __asan_init() does NOT use kernel32 VirtualAlloc to set up the shadow.
 *   It uses NtAllocateVirtualMemory (a direct ntdll syscall) and first calls
 *   VirtualQuery to verify the range is MEM_FREE.  If anything — including our
 *   own MEM_RESERVE — occupies the range, it aborts before any VirtualAlloc
 *   call is made.  An IAT hook on VirtualAlloc in the ASan DLL therefore
 *   cannot intercept shadow initialisation; it only sees heap/pool allocations
 *   (0x2F800000, 0x52000000, etc.) that are unrelated to the shadow mapping.
 *
 * Solution
 * --------
 * Phase A (DLL_PROCESS_ATTACH, before D3D/GPU init):
 *   VirtualAlloc([SHADOW_BASE, SHADOW_END), MEM_RESERVE, PAGE_NOACCESS).
 *   GPU anonymous pools cannot claim the shadow range.
 *   ASLR cannot place the ASan DLL image inside the shadow range.
 *
 * Phase B (via LdrRegisterDllNotification, fires BEFORE DllMain on Win10/11):
 *   When "clang_rt.asan_dynamic" is detected:
 *     - The DLL image is already placed safely outside shadow (Phase A ensured it).
 *     - Hook VirtualQuery in clang_rt's IAT.
 *     - When ASan's MemoryRangeIsAvailable calls VirtualQuery for the first
 *       shadow-range address (inside DllMain), our hook releases the reservation
 *       INSIDE that call — zero window between free and check.  Real VirtualQuery
 *       then returns MEM_FREE; the check passes; __asan_init maps the shadow.
 *
 *   Why not VirtualFree in ldr_notify directly?
 *     Tried and failed (PID 3428): ldr_notify fires before DllMain, VirtualFree
 *     succeeds ([vq-post-free]=FREE), yet ASan aborts (__asan_shadow_memory_-
 *     dynamic_address=0).  Between ldr_notify returning and DllMain being called,
 *     the loader expands the TLS slot array / does bookkeeping via HeapAlloc.
 *     That allocation lands at 0x2FFF0000+ (lowest free range), occupying the
 *     shadow range before ASan's check. VirtualQuery-inside-the-hook eliminates
 *     the window entirely.
 *
 * Diagnostic:
 *   Every event is logged to %TEMP%\asan_shadow_boot_debug.log.
 *   IAT hooks on LoadLibraryA/W in the EXE provide call-site logging only.
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
#include <wchar.h>

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

static char g_log_path[MAX_PATH];

static void
log_open(void)
{
    /*
     * The game install directory (CWD and DLL directory) is typically
     * UAC-protected on Vista+: user processes cannot write there without
     * elevation.  Use %TEMP% (C:\Users\<user>\AppData\Local\Temp\) which
     * is always writable by the current user.
     */
    DWORD n = GetTempPathA(MAX_PATH, g_log_path);
    if (n && n < MAX_PATH - 32) {
        strcat(g_log_path, "asan_shadow_boot_debug.log");
        g_log = fopen(g_log_path, "w");
    }

    /* Fallback 1: CWD (game install dir — may fail if UAC-protected) */
    if (!g_log) {
        strncpy(g_log_path, "asan_shadow_boot_debug.log", MAX_PATH - 1);
        g_log = fopen(g_log_path, "w");
    }

    /* Fallback 2: directory containing the DLL itself */
    if (!g_log) {
        DWORD m = GetModuleFileNameA(
            GetModuleHandleA("asan_shadow_boot.dll"), g_log_path, MAX_PATH);
        while (m > 0 && g_log_path[m-1] != '\\') --m;
        if (m) {
            g_log_path[m] = '\0';
            strcat(g_log_path, "asan_shadow_boot_debug.log");
            g_log = fopen(g_log_path, "w");
        }
    }
}


/* ------------------------------------------------------------------ */
/*  VirtualQuery helpers                                               */
/* ------------------------------------------------------------------ */

/*
 * Walk [SHADOW_BASE, 0x50000000) and log every region with its state,
 * type, protection, and allocation base.  Called before and after
 * VirtualFree to capture exactly what occupies the shadow range.
 *
 * Pre-free interpretation:
 *   - Only one MEM_RESERVE PRIVATE region (our own) → ldr_notify fires
 *     BEFORE DllMain.  After VirtualFree the range will be MEM_FREE.
 *   - Any MEM_COMMIT regions → ldr_notify fires AFTER DllMain; something
 *     has already been mapped in the shadow range.
 *
 * Post-free interpretation:
 *   - Single MEM_FREE region → VirtualFree succeeded; __asan_init will
 *     find the range clear.
 *   - Anything else → VirtualFree failed or something raced in.
 */
static void
vquery_shadow_diag(const char *tag)
{
    LPVOID addr = SHADOW_BASE;
    LPVOID end  = (LPVOID)0x50000000;
    int regions = 0;

    blog("[vq-%s] VirtualQuery scan [%p, 0x50000000):", tag, SHADOW_BASE);
    while (addr < end) {
        MEMORY_BASIC_INFORMATION mbi;
        SIZE_T ret = VirtualQuery(addr, &mbi, sizeof(mbi));
        if (!ret) {
            blog("[vq-%s]   VirtualQuery(%p) failed err=%lu", tag, addr,
                 (unsigned long)GetLastError());
            break;
        }

        const char *state =
            (mbi.State == MEM_FREE)    ? "FREE" :
            (mbi.State == MEM_RESERVE) ? "RESERVE" :
            (mbi.State == MEM_COMMIT)  ? "COMMIT" : "???";
        const char *type =
            (mbi.State == MEM_FREE)   ? "-" :
            (mbi.Type  == MEM_IMAGE)  ? "IMAGE" :
            (mbi.Type  == MEM_MAPPED) ? "MAPPED" :
            (mbi.Type  == MEM_PRIVATE)? "PRIVATE" : "???";

        blog("[vq-%s]   %p - %p  %-7s  %-7s  protect=%08lX  allocBase=%p",
             tag,
             mbi.BaseAddress,
             (LPVOID)((BYTE *)mbi.BaseAddress + mbi.RegionSize),
             state, type,
             (unsigned long)mbi.Protect,
             mbi.AllocationBase);
        ++regions;

        addr = (LPVOID)((BYTE *)mbi.BaseAddress + mbi.RegionSize);
    }
    blog("[vq-%s]   %d region(s) logged.", tag, regions);
}

/*
 * Scan the full 32-bit user address space for large (>= 128 MB) private
 * blocks that are MEM_RESERVE or MEM_COMMIT.  The ASan 32-bit shadow is
 * ~512 MB; this will locate it regardless of where the dynamic runtime
 * placed it.  Called once after CvGameCore initialises so that __asan_init
 * has either committed the shadow or aborted.
 *
 * Also reads __asan_shadow_memory_dynamic_address from clang_rt via
 * GetProcAddress — this is the runtime's own record of the shadow base.
 */
static void
find_asan_shadow(void)
{
    /* Read the symbol directly from the ASan runtime */
    HMODULE hasan = GetModuleHandleA("clang_rt.asan_dynamic-i386.dll");
    if (hasan) {
        FARPROC sym = GetProcAddress(hasan,
                          "__asan_shadow_memory_dynamic_address");
        if (sym)
            blog("[shadow-hunt] __asan_shadow_memory_dynamic_address"
                 " @ %p  value=0x%08lX",
                 (void *)sym, (unsigned long)*(DWORD *)sym);
        else
            blog("[shadow-hunt] __asan_shadow_memory_dynamic_address"
                 " not exported from clang_rt");
    } else {
        blog("[shadow-hunt] clang_rt.asan_dynamic-i386.dll not in process");
    }

    /* Walk the full user address space for large private blocks */
    blog("[shadow-hunt] Scanning user space for large (>=128MB) private"
         " blocks:");
    LPVOID addr = (LPVOID)0x00010000;
    LPVOID end  = (LPVOID)0xC0000000;  /* 32-bit user space ceiling */
    int    hits = 0;
    while (addr < end) {
        MEMORY_BASIC_INFORMATION mbi;
        SIZE_T ret = VirtualQuery(addr, &mbi, sizeof(mbi));
        if (!ret) break;

        if (mbi.RegionSize >= 0x8000000 &&       /* >= 128 MB            */
            mbi.State      != MEM_FREE   &&
            mbi.Type       == MEM_PRIVATE)
        {
            const char *state =
                (mbi.State == MEM_RESERVE) ? "RESERVE" :
                (mbi.State == MEM_COMMIT)  ? "COMMIT"  : "???";
            blog("[shadow-hunt]   %p - %p  size=0x%08lX  %s  "
                 "protect=%08lX  allocBase=%p",
                 mbi.BaseAddress,
                 (LPVOID)((BYTE *)mbi.BaseAddress + mbi.RegionSize),
                 (unsigned long)mbi.RegionSize,
                 state,
                 (unsigned long)mbi.Protect,
                 mbi.AllocationBase);
            ++hits;
        }
        addr = (LPVOID)((BYTE *)mbi.BaseAddress + mbi.RegionSize);
    }
    if (hits == 0)
        blog("[shadow-hunt]   No large private blocks found — "
             "ASan may have aborted before mapping shadow.");
    else
        blog("[shadow-hunt]   %d large private block(s) found.", hits);
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
    SetLastError(0);
    BOOL ok  = VirtualFree(SHADOW_BASE, 0, MEM_RELEASE);
    DWORD err = GetLastError();
    blog("[+] release_shadow via <%s>: VirtualFree(%p) = %s  err=%lu",
         trigger, SHADOW_BASE, ok ? "OK" : "FAILED", (unsigned long)err);
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
/*  VirtualQuery hook — installed in clang_rt's IAT at ldr_notify     */
/*                                                                     */
/*  Root cause (PID 3428 / __asan_shadow_memory_dynamic_address=0):   */
/*  ldr_notify fires before DllMain (proven by [vq-pre-free]).        */
/*  We called VirtualFree there; [vq-post-free] confirmed MEM_FREE.   */
/*  Yet ASan aborted.  The window:                                    */
/*                                                                     */
/*    ldr_notify returns                                               */
/*       |                                                             */
/*       +--> loader TLS slot expansion / bookkeeping (heap alloc)    */
/*       |    -> new heap segment lands at 0x2FFF0000+                */
/*       |       (lowest free range after our VirtualFree)            */
/*       |                                                             */
/*    DllMain called                                                   */
/*       |                                                             */
/*       +--> __asan_init -> MemoryRangeIsAvailable                   */
/*              -> VirtualQuery(0x2FFF0000) -> MEM_COMMIT             */
/*              -> "shadow interleaves" ABORT                         */
/*                                                                     */
/*  Fix: don't VirtualFree in ldr_notify.  Instead, hook VirtualQuery */
/*  in clang_rt's IAT.  When ASan's MemoryRangeIsAvailable calls      */
/*  VirtualQuery for the first shadow-range address, we release the   */
/*  reservation INSIDE that call — zero window between free and check.*/
/*  The hook is one-shot: IAT is restored immediately so subsequent   */
/*  VirtualQuery calls in the loop go to the real function.           */
/* ------------------------------------------------------------------ */

static FARPROC *g_vq_slot;
static FARPROC  g_real_vq;

static SIZE_T WINAPI
hooked_VirtualQuery(LPCVOID lpAddress,
                    PMEMORY_BASIC_INFORMATION lpBuffer,
                    SIZE_T dwLength)
{
    DWORD addr = (DWORD)(DWORD_PTR)lpAddress;

    if (addr >= (DWORD)(DWORD_PTR)SHADOW_BASE && addr < 0x50000000u) {
        blog("[VQ-hook] VirtualQuery(%p) in shadow range — releasing reservation",
             lpAddress);

        /* Restore real VirtualQuery first to avoid any re-entrancy */
        FARPROC *slot = g_vq_slot;
        if (slot) {
            patch_iat(slot, g_real_vq, NULL);
            g_vq_slot = NULL;
        }

        /* Release the reservation so the real VirtualQuery sees MEM_FREE */
        release_shadow("VirtualQuery-hook");
    }

    SIZE_T ret = ((SIZE_T (WINAPI *)(LPCVOID, PMEMORY_BASIC_INFORMATION, SIZE_T))
                  g_real_vq)(lpAddress, lpBuffer, dwLength);

    /* Log what ASan receives for the first shadow-range query so we can
     * confirm MEM_FREE is returned (the check will pass) vs anything else
     * (the check will fail and ASan will abort). */
    if (addr >= (DWORD)(DWORD_PTR)SHADOW_BASE && addr < 0x50000000u && ret && lpBuffer) {
        const char *state =
            lpBuffer->State == MEM_FREE    ? "FREE" :
            lpBuffer->State == MEM_RESERVE ? "RESERVE" :
            lpBuffer->State == MEM_COMMIT  ? "COMMIT"  : "???";
        blog("[VQ-hook] result for %p: state=%s  size=0x%08lX",
             lpAddress, state, (unsigned long)lpBuffer->RegionSize);
    }

    return ret;
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
    return (int)strlen(name) >= prefix_len &&
           _strnicmp(name, prefix, (size_t)prefix_len) == 0;
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
    char narrow[MAX_PATH];
    int is_mods_cvgame = 0;

    if (name) {
        WideCharToMultiByte(CP_UTF8, 0, name, -1, narrow, MAX_PATH, NULL, NULL);
        blog("[diag-W] LoadLibraryW(\"%s\")", narrow);
        /* Detect the MODS CvGameCore (ASan build): path contains both a
         * mod-directory marker and the CvGameCore DLL name. */
        is_mods_cvgame = (strstr(narrow, "MODS") || strstr(narrow, "mods")) &&
                          name_starts_a(narrow, CVGAME_PREFIX, CVGAME_PFX_LEN);
    }

    HMODULE result = ((HMODULE(WINAPI*)(LPCWSTR))g_real_w)(name);

    /* After the real LoadLibraryW returns for the MODS CvGameCore, every
     * DllMain, ldr_notify callback, and TLS callback has completed.  If
     * ASan initialised successfully the shadow is now committed and
     * __asan_shadow_memory_dynamic_address is non-zero.  This is the
     * definitive "did ASan succeed?" checkpoint. */
    if (is_mods_cvgame) {
        DWORD load_err = GetLastError();
        blog("[post-load] LoadLibraryW(\"%s\") = %p  GetLastError=%lu",
             narrow, (void *)result, (unsigned long)load_err);

        /* If LoadLibraryW returned NULL, check whether the DLL is still
         * accessible via GetModuleHandle.  This disambiguates:
         *   handle != NULL -> DLL IS loaded; game uses GetModuleHandle,
         *                     not LoadLibraryW return value.  ASan running.
         *   handle == NULL -> DLL NOT in process; true load failure.     */
        if (!result) {
            HMODULE hmod = GetModuleHandleW(L"CvGameCore_Expansion2.dll");
            blog("[post-load] GetModuleHandleW(CvGameCore_Expansion2) = %p",
                 (void *)hmod);
        }

        blog("[post-load] All init complete (DllMain + ldr_notify + TLS).");
        find_asan_shadow();
    }

    return result;
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
/*                                                                     */
/*  UNICODE_STRING / NTSTATUS / NTAPI live in winternl.h / ntdef.h    */
/*  which are NOT pulled in by WIN32_LEAN_AND_MEAN.  Define the        */
/*  minimal types privately with an AB_ prefix to be 100% conflict-   */
/*  free regardless of which Windows SDK version is on the build host. */
/* ------------------------------------------------------------------ */

typedef struct {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} AB_UNICODE_STRING, *PAB_UNICODE_STRING;

typedef LONG AB_NTSTATUS;
#define AB_NTAPI __stdcall

typedef struct {
    ULONG                Flags;
    PAB_UNICODE_STRING   FullDllName;
    PAB_UNICODE_STRING   BaseDllName;
    PVOID                DllBase;
    ULONG                SizeOfImage;
} MY_LDR_DATA;

typedef VOID          (CALLBACK     *MY_NOTIFY_FN)(ULONG, MY_LDR_DATA *, PVOID);
typedef AB_NTSTATUS   (AB_NTAPI *PFN_REGISTER)    (ULONG, MY_NOTIFY_FN, PVOID, PVOID *);

#define LDR_DLL_NOTIFICATION_REASON_LOADED   1
#define LDR_DLL_NOTIFICATION_REASON_UNLOADED 2

static PVOID g_ldr_cookie;

static int
us_starts_w(PAB_UNICODE_STRING us, const WCHAR *prefix, int prefix_len)
{
    if (!us || !us->Buffer) return 0;
    int len = us->Length / (int)sizeof(WCHAR);
    if (len < prefix_len) return 0;
    return _wcsnicmp(us->Buffer, prefix, (size_t)prefix_len) == 0;
}

static VOID CALLBACK
ldr_notify(ULONG reason, MY_LDR_DATA *data, PVOID ctx)
{
    (void)ctx;
    if (reason != LDR_DLL_NOTIFICATION_REASON_LOADED) return;
    if (!data || !data->BaseDllName || !data->BaseDllName->Buffer) return;

    PAB_UNICODE_STRING n = data->BaseDllName;
    int len = n->Length / (int)sizeof(WCHAR);
    blog("[B1] DLL loaded: %.*ls  base=%p  size=0x%lX",
         len, n->Buffer, data->DllBase, data->SizeOfImage);

    /* Diagnostic: log vanilla/mod CvGameCore loads.
     * For the mod (ASAN-instrumented) CvGameCore, run the full shadow-hunt:
     *   - vquery_shadow_diag: confirms state of [0x2FFF0000, 0x50000000)
     *   - find_asan_shadow:   scans the FULL address space for the actual
     *     shadow block and reads __asan_shadow_memory_dynamic_address.
     * By this point clang_rt DllMain (__asan_init) has already run, so the
     * shadow is either committed somewhere or ASan aborted. */
    if (us_starts_w(n, CVGAME_PREFIX_W, CVGAME_PFX_LEN)) {
        if (data->SizeOfImage > 0x1000000) {   /* mod/ASAN build is very large */
            blog("     -> CvGameCore_Expansion2 (mod/ASAN build) detected"
                 "  base=%p  size=0x%lX", data->DllBase, data->SizeOfImage);
            vquery_shadow_diag("post-CvGame");
            find_asan_shadow();
        } else {
            blog("     -> CvGameCore_Expansion2 (factory/DLC) detected"
                 " (shadow kept reserved)");
        }
        return;
    }

    /* Primary trigger: clang_rt.asan_dynamic — fires BEFORE DllMain.
     *
     * Strategy: hook VirtualQuery in clang_rt's IAT.
     *   - ASan's MemoryRangeIsAvailable calls VirtualQuery for each page of
     *     the shadow range.  Our hook fires on the FIRST such call, releases
     *     the reservation inside that call (zero window), then restores the
     *     real VirtualQuery so subsequent loop iterations go directly.
     *   - Fallback: if VirtualQuery is not in clang_rt's IAT, call
     *     release_shadow now (old approach; window exists but rare). */
    if (us_starts_w(n, ASAN_RT_PREFIX_W, ASAN_RT_PFX_LEN)) {
        blog("     -> ASan runtime detected  base=%p  size=0x%lX",
             data->DllBase, data->SizeOfImage);

        vquery_shadow_diag("pre-hook");   /* confirm only our RESERVE is present */

        FARPROC *vq_slot = find_iat_slot_in(data->DllBase,
                               "KERNEL32.DLL", "VirtualQuery");
        if (vq_slot) {
            patch_iat(vq_slot, (FARPROC)hooked_VirtualQuery, &g_real_vq);
            g_vq_slot = vq_slot;
            blog("     VirtualQuery IAT hooked @ %p  real=%p", vq_slot, g_real_vq);
            blog("     Shadow will be released inside first VirtualQuery"
                 " for shadow range.");
            /* Do NOT call release_shadow here — hook fires at the right moment */
        } else {
            blog("     WARNING: VirtualQuery not in clang_rt IAT — "
                 "releasing now (window may exist)");
            release_shadow("ldr_notify-fallback");
            vquery_shadow_diag("post-free-fallback");
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
    AB_NTSTATUS st = fn(0, ldr_notify, NULL, &g_ldr_cookie);
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
    blog("[A] Log: %s", g_log_path[0] ? g_log_path : "(failed to open)");

    /* Phase A: reserve shadow range before D3D/GPU init */
    LPVOID res = VirtualAlloc(SHADOW_BASE, SHADOW_SIZE, MEM_RESERVE, PAGE_NOACCESS);
    blog("[A] VirtualAlloc(SHADOW_BASE, 512MB, MEM_RESERVE): %s  addr=%p",
         res ? "OK" : "FAILED (range already occupied)", res);

    /* Phase B: LdrDllNotification -> release shadow when clang_rt is detected */
    register_ldr_notification();

    /* Diagnostic: log all LoadLibraryA/W calls from EXE (no release here) */
    install_loadlibrary_diag();

    blog("[A] Init complete. Shadow stays reserved until clang_rt.asan_dynamic loads.");
    return TRUE;
}
