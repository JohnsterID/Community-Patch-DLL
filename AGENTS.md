# AGENTS.md -- Repository Knowledge Base

## Git Authorship
All commits must use:
- **Author/Committer:** `JohnsterID <69278611+JohnsterID@users.noreply.github.com>`
- **No** `Co-authored-by:` lines
- See `/workspace/project/GIT_COMMIT_AUTHORSHIP_INSTRUCTIONS.md` for full details

```bash
git config user.name "JohnsterID"
git config user.email "69278611+JohnsterID@users.noreply.github.com"
```

---

## Clang/Linux Build (`clang-linux` branch)

### Prerequisites
- **Dependencies zip:** `/workspace/project/v90-dependencies.zip`
  - Extract so that `v90-dependencies/Dependencies/` lands at `./Dependencies/`
  - Contains: `v7.0a_include/`, `v7.0a_lib/`, `vc9_include/`, `vc9_lib/`
- **System packages:** `clang lld` (via `sudo apt-get install -y clang lld`)
- **LLVM 21.1.8:** https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.8/LLVM-21.1.8-Linux-X64.tar.xz
  - Extract to `/tmp/LLVM-21.1.8-Linux-X64`
  - The build script defaults to this path via `LLVM_PATH` env var

### Build Steps
```bash
# 1. Extract dependencies
python3 -c "
import zipfile, shutil
z = zipfile.ZipFile('/workspace/project/v90-dependencies.zip')
z.extractall('/tmp/v90-dep-extract')
shutil.copytree('/tmp/v90-dep-extract/v90-dependencies/Dependencies',
                'Dependencies')
"

# 2. Fix header case issues (Linux case-sensitive fs vs Windows mixed-case headers)
python3 fix_header_case_issues.py

# 3. Build (run sequentially to avoid OOM -- parallel builds crash the session)
export LLVM_PATH=/tmp/LLVM-21.1.8-Linux-X64
python3 build_vp_clang_linux.py --config debug
python3 build_vp_clang_linux.py --config release
```

### Build Output
- `clang-output/Debug/CvGameCore_Expansion2.dll` (~24 MB)
- `clang-output/Release/CvGameCore_Expansion2.dll` (~13 MB)
- `clang-output/{Debug,Release}/build.log`
- Build times: ~2 min Debug, ~2.5 min Release (single-threaded CPP compilation)

### WARNING Do NOT run both configs in parallel -- causes OOM/session crash

---

## Known Build Warnings (clang-linux, as of 2026-05-01)

Warnings are **identical** between Debug and Release (1,104 total). All are pre-existing.

### Compiler Warnings
| Count | Warning | Location | Notes |
|-------|---------|----------|-------|
| 1,042 | `-Woverloaded-virtual` | All files (via PCH) | 7 virtual functions hide overloaded base versions in `ICvNetMessageHandler2`, `ICvGame2`, `CvPlayerTechs`, `CvPlayerPolicies`, `CvCityStrategyAI`, `CvWonderProductionAI` |
| 4 | `-Wmisleading-indentation` | `CvMinorCivAI.cpp:18558,18564,18570`; `CvUnit.cpp:28626` | **Actionable** -- potential logic bugs |
| 15 | `-Wunused-parameter` | Various files | Low priority |
| 2 | `-Wsign-compare` | `CvWorldBuilderMapLoader.cpp:975-976` | `uint` vs `int` |
| 1 | `-Wmissing-field-initializers` | Unknown | `bIsLocalizedText` field |

### Linker Warnings
| Warning | Cause | Impact |
|---------|-------|--------|
| `LNK4099` (28x) | No PDB for precompiled libs (`FireWorksWin32.obj`, `CvGameCoreDLLUtilWin32.lib`, `CvWorldBuilderMapWin32.obj`, `FLuaWin32.lib`) | None -- expected for third-party precompiled objects |
| `duplicate symbol: CvAssertDlg` | `clang.obj` overlap | Tolerated by `/FORCE:MULTIPLE` |
| `undefined symbol: operator delete(void*, unsigned int)` and `operator delete[](void*, unsigned int)` | Sized deallocation (C++14) not in VC9 CRT; from `CvLuaCity.obj` | Tolerated by `/FORCE:UNRESOLVED` |

### False Positives in `grep "error:"`
Lines containing `error:` in the logs are **not actual errors** -- they are `>>>` reference lines inside `lld-link: warning:` messages, showing which destructor sites reference the undefined sized `operator delete`. Both builds link successfully.

---

## Minidump Analysis (scripts/analyze_minidump.py)

Cross-platform crash-dump analyzer; full usage in `docs/minidumps.md`.
```bash
python3 scripts/analyze_minidump.py CvMiniDump_*.dmp \
    --symbols <extracted Release_Debug.zip dir> --crashes-log crashes.log
```
- Handles Wine-generated dumps (nonstandard 0xfff0 stream breaks the python
  `minidump` lib; this tool walks the header/directory manually).
- Auto-pairs dump module -> DLL by PE timestamp + SizeOfImage, DLL -> PDB by
  RSDS GUID+age (Standard vs 43 Civ DLLs differ only in timestamp).
- Symbolization backends: `llvm-symbolizer` (LLVM >= 19; file:line + inlined
  frames from PDB) with pure-Python PDB reader fallback (MSF/DBI walk,
  S_GPROC32/S_LPROC32 + publics; slower, but zero dependencies).
  Symbolizer found via `--llvm-path`, `$LLVM_PATH/bin`, then `$PATH`.
  A big LLVM tarball may exist at `/workspace/project/LLVM-*-Linux-X64.tar.xz`;
  extracting just `bin/llvm-symbolizer` is enough (~10 MB vs ~2 GB).
- crashes.log "Location (in file)" is a FILE OFFSET; the tool converts to true
  RVA via the DLL's .text raw->virtual delta (typically +0xC00).
- Largest-free-block < a few MB in crashes.log = 32-bit address-space
  exhaustion (OOM class).
- Stack-scan candidates for the target DLL (and any `--image` module, e.g.
  the game exe from `/workspace/project/Sid Meier's Civilization V/`) are
  verified as call return addresses (executable section + preceding call
  insn); this removes most false positives (data pointers into module ranges).
- crashes.log `???+0xfffffXXX` entries = EIP outside every module (call
  through NULL/garbage pointer); no RVA fixup applies.
- EXE address cross-reference (verified 2026-08-01): Windows exes have PE
  ImageBase 0x400000, so exe+RVA -> sub_(0x400000+RVA) in
  CivilizationV*.exe.c. The Civ5XP ELF loads at 0x08048000 (NOT 0x400000 --
  the note in minidump-pdb-plan.txt is wrong on this); Civ5XP.c function
  addresses are absolute ELF vaddrs. Match Windows sub_XXXXXX to Civ5XP.c
  named functions by structural idiom search (shared strings are ambiguous:
  same SQL appears in multiple functions). Confirmed pairing: the EXE event
  dispatch loop sub_420F40 (DX9) / sub_6A5970 (DX11) =
  GameCoreEventDispatcher<GameCoreEventQueue<EventStream>,DispatchData>::
  DispatchEvents (Civ5XP 0x86EF766); status.txt Session-2 ret addr
  exe+0x20F75 verified in exe bytes as directly after FF D0 (call eax).
- Validated against issue #13254 dump (crash RVA 0xA57CF0 ->
  `std::vector<CvTacticalPlot>::_Ufill`, CvTacticalAI.h:855 inlined chain)
  and issue #13262 dump (EIP=0 execute-DEP AV during save load; raw scan had
  546 bogus CvGameCore hits, call-site verification leaves ~40; exe frames
  all data refs -> crash is in EXE-side dispatch before reaching our DLL,
  same class as the 11:32 Session-2 load crash in status.txt).
