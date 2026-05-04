/*
 * asan_launcher.exe
 *
 * Injects asan_shadow_boot.dll into CivilizationV.exe before the main thread
 * starts, reserving the ASan shadow range before D3D/GPU driver initialisation
 * can claim it.  No registry changes required.
 *
 * Based on the CREATE_SUSPENDED + CreateRemoteThread(LoadLibraryA) pattern
 * from Zenith-test/Zenith/zenith_launcher.cpp (SetSail DLL injection).
 *
 * Usage
 * -----
 *   asan_launcher.exe  <path\to\CivilizationV.exe>
 *   set CIVV_EXE=<path\to\CivilizationV.exe> && asan_launcher.exe
 *
 *   The EXE path is required.  No auto-detection is attempted because the
 *   system may have multiple Civ5 copies (Steam, GOG, non-Steam).
 *
 *   asan_shadow_boot.dll must live in the same directory as this launcher.
 *   Both are placed in clang-output\Debug\ by build_vp_clang.py.
 *
 * Debug output
 * ------------
 *   Messages go to the console AND asan_launcher_debug.log in the launcher
 *   directory, which survives even if the console window closes.
 *
 * Build (automatic via build_vp_clang.py --sanitizer asan):
 *   clang-cl /nologo /W3 /O2 /MT /GS- asan_launcher.c /Fo:... /c
 *   lld-link  /nologo /MACHINE:x86 /SUBSYSTEM:CONSOLE ... kernel32.lib advapi32.lib
 *
 * Must be 32-bit (/MACHINE:x86): GetProcAddress("LoadLibraryA") must return
 * the 32-bit kernel32 address valid in the 32-bit game process.  On 64-bit
 * Windows, system-DLL ASLR bases are randomised once per boot and are
 * identical for all same-bitness processes.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

#define BOOT_DLL_NAME     "asan_shadow_boot.dll"
#define LOG_FILE_NAME     "asan_launcher_debug.log"
#define GAME_EXE_NAME     "CivilizationV.exe"
#define INJECT_TIMEOUT_MS 10000   /* ms to wait for LoadLibraryA */

#define SHADOW_BASE_STR   "0x2FFF0000"
#define SHADOW_END_STR    "0x50000000"


/* ------------------------------------------------------------------ */
/*  Dual logger: console + file                                        */
/* ------------------------------------------------------------------ */

static FILE *g_log;

static void
log_open(const char *dir)
{
    char path[MAX_PATH];
    _snprintf(path, sizeof path, "%s\\%s", dir, LOG_FILE_NAME);
    g_log = fopen(path, "w");
    if (!g_log) fprintf(stderr, "[warn] Cannot open log: %s\n", path);
}

static void
vlog(const char *pfx, const char *fmt, va_list ap)
{
    char buf[2048];
    _vsnprintf(buf, sizeof buf, fmt, ap);
    printf("%s%s\n", pfx, buf);
    fflush(stdout);
    if (g_log) { fprintf(g_log, "%s%s\n", pfx, buf); fflush(g_log); }
}

static void log_info(const char *f,...){va_list a;va_start(a,f);vlog("[*] ",f,a);va_end(a);}
static void log_ok  (const char *f,...){va_list a;va_start(a,f);vlog("[+] ",f,a);va_end(a);}
static void log_warn(const char *f,...){va_list a;va_start(a,f);vlog("[!] ",f,a);va_end(a);}
static void log_err (const char *f,...){va_list a;va_start(a,f);vlog("[-] ",f,a);va_end(a);}
static void log_dbg (const char *f,...){va_list a;va_start(a,f);vlog("    ",f,a);va_end(a);}


/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static const char *
winerr(DWORD code)
{
    switch (code) {
        case   2: return "ERROR_FILE_NOT_FOUND";
        case   5: return "ERROR_ACCESS_DENIED";
        case 126: return "ERROR_MOD_NOT_FOUND (missing dependency)";
        case 193: return "ERROR_BAD_EXE_FORMAT (wrong architecture)";
        case 1114:return "ERROR_DLL_INIT_FAILED (DllMain returned FALSE)";
        default:  return "";
    }
}

static void
get_launcher_dir(char *out)
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    char *sep = strrchr(path, '\\');
    if (sep) *sep = '\0';
    strncpy(out, path, MAX_PATH - 1);
}

/* Read PE machine word for architecture diagnostics */
static WORD
pe_machine(const char *path)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    IMAGE_DOS_HEADER dos; DWORD rd;
    if (!ReadFile(h, &dos, sizeof dos, &rd, NULL) ||
        dos.e_magic != IMAGE_DOS_SIGNATURE) { CloseHandle(h); return 0; }
    SetFilePointer(h, dos.e_lfanew, NULL, FILE_BEGIN);
    IMAGE_NT_HEADERS nt;
    if (!ReadFile(h, &nt, sizeof nt, &rd, NULL) ||
        nt.Signature != IMAGE_NT_SIGNATURE)  { CloseHandle(h); return 0; }
    CloseHandle(h);
    return nt.FileHeader.Machine;
}


/* ------------------------------------------------------------------ */
/*  DLL injection                                                      */
/*                                                                     */
/*  Matches the pattern in Zenith-test zenith_launcher.cpp:           */
/*    VirtualAllocEx + WriteProcessMemory +                           */
/*    CreateRemoteThread(LoadLibraryA) +                               */
/*    WaitForSingleObject + GetExitCodeThread                          */
/* ------------------------------------------------------------------ */

static BOOL
inject_dll(HANDLE proc, const char *dll_abs)
{
    log_info("Injection: %s", dll_abs);

    /* Verify DLL exists, log its size and modification time */
    HANDLE hf = CreateFileA(dll_abs, GENERIC_READ, FILE_SHARE_READ,
                            NULL, OPEN_EXISTING, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        log_err("Cannot open DLL: %lu %s", e, winerr(e));
        return FALSE;
    }
    DWORD dll_sz = GetFileSize(hf, NULL);

    FILETIME ft_write; SYSTEMTIME st_write;
    if (GetFileTime(hf, NULL, NULL, &ft_write) &&
        FileTimeToSystemTime(&ft_write, &st_write)) {
        log_dbg("DLL file size:     %lu bytes", dll_sz);
        log_dbg("DLL last modified: %04d-%02d-%02d %02d:%02d:%02d UTC",
                st_write.wYear, st_write.wMonth, st_write.wDay,
                st_write.wHour, st_write.wMinute, st_write.wSecond);
    } else {
        log_dbg("DLL file size: %lu bytes", dll_sz);
    }
    CloseHandle(hf);

    /*
     * The original DLL (first commit, no logging/hook code) is 8704 bytes.
     * The current DLL with LdrDllNotification + VirtualQuery hook + file
     * logging is substantially larger.  Warn if the file looks stale.
     */
    if (dll_sz <= 9000) {
        log_warn("DLL is only %lu bytes — this looks like a stale build.", dll_sz);
        log_warn("  The current version has logging, LdrDllNotification,");
        log_warn("  and a VirtualQuery hook; it will be significantly larger.");
        log_warn("  Rebuild: python build_vp_clang.py --sanitizer asan");
        log_warn("  Then copy asan_shadow_boot.dll to the game directory.");
    }

    /* Verify DLL is x86 */
    WORD mach = pe_machine(dll_abs);
    log_dbg("DLL machine:  0x%04X (%s)", mach,
            mach == IMAGE_FILE_MACHINE_I386  ? "x86 OK"    :
            mach == IMAGE_FILE_MACHINE_AMD64 ? "x64 WRONG" : "unknown");
    if (mach && mach != IMAGE_FILE_MACHINE_I386) {
        log_err("DLL is not x86 — must be built with /MACHINE:x86");
        return FALSE;
    }

    /* Allocate remote buffer for the DLL path (ANSI, for LoadLibraryA) */
    SIZE_T path_len = strlen(dll_abs) + 1;
    LPVOID rbuf = VirtualAllocEx(proc, NULL, path_len,
                                 MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!rbuf) {
        DWORD e = GetLastError();
        log_err("VirtualAllocEx failed: %lu %s", e, winerr(e));
        return FALSE;
    }
    log_dbg("Remote buffer: 0x%08X (%Iu bytes)", (DWORD)(DWORD_PTR)rbuf, path_len);

    SIZE_T written = 0;
    if (!WriteProcessMemory(proc, rbuf, dll_abs, path_len, &written)) {
        DWORD e = GetLastError();
        log_err("WriteProcessMemory failed: %lu %s", e, winerr(e));
        VirtualFreeEx(proc, rbuf, 0, MEM_RELEASE);
        return FALSE;
    }
    log_dbg("Path written:  %Iu bytes", written);

    /*
     * LoadLibraryA address from our 32-bit process is valid in the 32-bit
     * game process: Windows randomises system-DLL ASLR bases once per boot,
     * identically across all same-bitness processes.
     */
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    FARPROC lla = GetProcAddress(k32, "LoadLibraryA");
    log_dbg("kernel32!LoadLibraryA: 0x%08X", (DWORD)(DWORD_PTR)lla);

    HANDLE thr = CreateRemoteThread(proc, NULL, 0,
                                    (LPTHREAD_START_ROUTINE)(DWORD_PTR)lla,
                                    rbuf, 0, NULL);
    if (!thr) {
        DWORD e = GetLastError();
        log_err("CreateRemoteThread failed: %lu %s", e, winerr(e));
        VirtualFreeEx(proc, rbuf, 0, MEM_RELEASE);
        return FALSE;
    }
    log_info("Remote thread created — waiting for LoadLibraryA to return ...");

    DWORD wait = WaitForSingleObject(thr, INJECT_TIMEOUT_MS);
    if (wait == WAIT_TIMEOUT) {
        log_err("Timed out (%d ms)", INJECT_TIMEOUT_MS);
        CloseHandle(thr);
        VirtualFreeEx(proc, rbuf, 0, MEM_RELEASE);
        return FALSE;
    }

    DWORD exit_code = 0;
    GetExitCodeThread(thr, &exit_code);
    CloseHandle(thr);
    VirtualFreeEx(proc, rbuf, 0, MEM_RELEASE);

    log_dbg("LoadLibraryA returned HMODULE = 0x%08X", exit_code);

    if (!exit_code) {
        log_err("LoadLibraryA returned NULL — DLL load failed in target process");
        /* Try loading in our own process to surface a better error code */
        HMODULE test = LoadLibraryA(dll_abs);
        if (!test) {
            DWORD e = GetLastError();
            log_dbg("  Local LoadLibraryA also failed: %lu %s", e, winerr(e));
            if (e == 1114) log_dbg("  -> DllMain returned FALSE");
            if (e == 126)  log_dbg("  -> Missing CRT or other dependency");
            if (e == 193)  log_dbg("  -> Wrong architecture (need x86 DLL)");
        } else {
            log_dbg("  Local load succeeded — remote failure may be a permission issue");
            FreeLibrary(test);
        }
        return FALSE;
    }

    log_ok("DLL loaded. HMODULE = 0x%08X", exit_code);
    return TRUE;
}


/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    char launcher_dir[MAX_PATH];
    get_launcher_dir(launcher_dir);
    log_open(launcher_dir);

    printf("\n");
    log_info("asan_launcher.exe  --  ASan shadow-range injection launcher");
    log_info("Log file: %s\\%s", launcher_dir, LOG_FILE_NAME);
    printf("\n");

    /* ----------------------------------------------------------------
     * 1.  Resolve the game EXE path
     * ---------------------------------------------------------------- */

    char game_exe[MAX_PATH] = {0};

    if (argc >= 2) {
        /* Explicit argument */
        GetFullPathNameA(argv[1], MAX_PATH, game_exe, NULL);
        if (GetFileAttributesA(game_exe) == INVALID_FILE_ATTRIBUTES) {
            log_err("Path not found: %s", game_exe);
            return 1;
        }
    } else {
        /* Fall back to CIVV_EXE environment variable */
        char env[MAX_PATH];
        if (GetEnvironmentVariableA("CIVV_EXE", env, MAX_PATH)) {
            GetFullPathNameA(env, MAX_PATH, game_exe, NULL);
            if (GetFileAttributesA(game_exe) == INVALID_FILE_ATTRIBUTES) {
                log_err("%%CIVV_EXE%% path not found: %s", game_exe);
                return 1;
            }
            log_info("Using %%CIVV_EXE%%: %s", game_exe);
        } else {
            log_err("No game path provided.");
            log_dbg("  asan_launcher.exe \"C:\\...\\CivilizationV.exe\"");
            log_dbg("  -- or --");
            log_dbg("  set CIVV_EXE=C:\\...\\CivilizationV.exe && asan_launcher.exe");
            return 1;
        }
    }

    WORD game_mach = pe_machine(game_exe);
    log_info("Game EXE: %s", game_exe);
    log_dbg("  Machine: 0x%04X (%s)", game_mach,
            game_mach == IMAGE_FILE_MACHINE_I386  ? "x86"  :
            game_mach == IMAGE_FILE_MACHINE_AMD64 ? "x64"  : "?");
    if (game_mach && game_mach != IMAGE_FILE_MACHINE_I386)
        log_warn("EXE is not x86 — injection still attempted");

    /* ----------------------------------------------------------------
     * 2.  Boot DLL must sit next to this launcher
     * ---------------------------------------------------------------- */

    char dll_abs[MAX_PATH];
    _snprintf(dll_abs, sizeof dll_abs, "%s\\%s", launcher_dir, BOOT_DLL_NAME);

    if (GetFileAttributesA(dll_abs) == INVALID_FILE_ATTRIBUTES) {
        log_err("Boot DLL not found: %s", dll_abs);
        log_dbg("  Run:  python build_vp_clang.py --sanitizer asan");
        return 1;
    }
    log_info("Boot DLL: %s", dll_abs);

    /* Game CWD = parent dir of the EXE so it finds its own DLLs */
    char game_dir[MAX_PATH];
    strncpy(game_dir, game_exe, MAX_PATH - 1);
    char *sep = strrchr(game_dir, '\\');
    if (sep) *sep = '\0';

    /* ----------------------------------------------------------------
     * 3.  Create the game process in a suspended state
     * ---------------------------------------------------------------- */

    printf("\n");
    log_info("Creating process (suspended):");
    log_dbg("  EXE: %s", game_exe);
    log_dbg("  CWD: %s", game_dir);

    STARTUPINFOA si;        ZeroMemory(&si, sizeof si); si.cb = sizeof si;
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof pi);

    if (!CreateProcessA(game_exe, NULL, NULL, NULL,
                        FALSE, CREATE_SUSPENDED,
                        NULL, game_dir, &si, &pi)) {
        DWORD e = GetLastError();
        log_err("CreateProcessA failed: %lu %s", e, winerr(e));
        if (e == 5) log_dbg("  -> Try running this launcher as Administrator.");
        return 1;
    }

    log_ok("PID %lu created (main thread suspended)", pi.dwProcessId);
    log_dbg("  hProcess: 0x%08X   hThread: 0x%08X",
            (DWORD)(DWORD_PTR)pi.hProcess, (DWORD)(DWORD_PTR)pi.hThread);

    /* ----------------------------------------------------------------
     * 4.  Inject asan_shadow_boot.dll
     *     Phase A (DllMain):        VirtualAlloc shadow range — GPU/ASLR
     *                               cannot claim it before clang_rt loads.
     *     Phase B (ldr_notify +     Detect clang_rt; hook VirtualQuery in
     *              VirtualQuery      its IAT; release reservation INSIDE
     *              IAT hook):        the first VirtualQuery for the shadow
     *                               range (zero window — no heap races).
     * ---------------------------------------------------------------- */

    printf("\n");
    log_info("--- Injecting %s ---", BOOT_DLL_NAME);
    BOOL ok = inject_dll(pi.hProcess, dll_abs);
    printf("\n");

    if (ok) {
        log_ok("Injection succeeded.");
        log_info("Phase A: VirtualAlloc [%s, %s) — result in DLL log.",
                 SHADOW_BASE_STR, SHADOW_END_STR);
        log_info("Phase B: LdrDllNotification + VirtualQuery hook registered.");
        log_info("DLL log written to %%TEMP%%\\asan_shadow_boot_debug.log");
        {
            char tmp[MAX_PATH];
            if (GetTempPathA(MAX_PATH, tmp))
                log_info("         -> %sasan_shadow_boot_debug.log", tmp);
        }
    } else {
        log_warn("Injection failed — resuming game anyway.");
        log_warn("ASan may abort if GPU drivers occupy [%s, %s).",
                 SHADOW_BASE_STR, SHADOW_END_STR);
    }

    /* ----------------------------------------------------------------
     * 5.  Resume the game's main thread
     * ---------------------------------------------------------------- */

    printf("\n");
    log_info("Resuming main thread ...");
    ResumeThread(pi.hThread);
    log_ok("Game is running (PID %lu). Launcher exiting.", pi.dwProcessId);
    log_info("Workflow: main menu -> mod menu -> activate VP mod -> play.");
    log_info("ASan output: set ASAN_OPTIONS=log_path=asan_game.log:halt_on_error=0");

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (g_log) fclose(g_log);

    /* Exit codes: 0 = fully OK, 1 = failed before launch, 2 = launched but inject failed */
    return ok ? 0 : 2;
}
