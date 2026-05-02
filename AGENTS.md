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
- **LLVM 22.1.4:** https://github.com/llvm/llvm-project/releases/download/llvmorg-22.1.4/LLVM-22.1.4-Linux-X64.tar.xz
  - Extract to `/tmp/LLVM-22.1.4-Linux-X64`
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
export LLVM_PATH=/tmp/LLVM-22.1.4-Linux-X64
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

## Known Build Warnings (clang-linux, LLVM 22.1.4, as of 2026-05-02)

Warnings are **identical** between Debug and Release (1,276 total). All are pre-existing.

### Compiler Warnings
| Count | Warning | Location | Notes |
|-------|---------|----------|-------|
| 1,042 | `-Woverloaded-virtual` | All files (via PCH) | 7 virtual functions hide overloaded base versions in `ICvNetMessageHandler2`, `ICvGame2`, `CvPlayerTechs`, `CvPlayerPolicies`, `CvCityStrategyAI`, `CvWonderProductionAI` |
| 3 | `-Wdangling-else` | `CvTacticalAI.cpp:11748,11754,11760` | **New in 22.1.4** -- ambiguous if/else nesting (code smell, not a logic bug) |
| 1 | `-Wmisleading-indentation` | `CvUnit.cpp:28921` | Mixed tab/space indent |
| 1 | `-Wmissing-field-initializers` | `CvInfos.cpp:416` | `bIsLocalizedText` field |
| 23 | `-Wunused-parameter` | Various Dll interface files | Low priority |
| 2 | `-Wsign-compare` | `CvWorldBuilderMapLoader.cpp:975-976` | `uint` vs `int` |

### Linker Warnings
| Warning | Cause | Impact |
|---------|-------|--------|
| `LNK4099` (28x) | No PDB for precompiled libs | None -- expected |
| `duplicate symbol: CvAssertDlg` | `clang.obj` overlap | Tolerated by `/FORCE:MULTIPLE` |
| `duplicate symbol: std::swap<SUnitIDValueContainer>` (171x) | Template instantiation in multiple TUs | **New in 22.1.4** -- tolerated by `/FORCE:MULTIPLE` |
| `undefined symbol: operator delete(void*, unsigned int)` (2x) | Sized deallocation (C++14) not in VC9 CRT | Tolerated by `/FORCE:UNRESOLVED` |

### Clang Static Analyzer Results (LLVM 22.1.4)
Run via `python3 build_vp_clang_linux.py --config debug --analyze`:
| Count | Check | Notes |
|-------|-------|-------|
| 56 | `core.CallAndMessage` (null deref) | ~14 real, rest ASSERT-protected or contract-enforced |
| 5 | `core.DivideZero` | 1 real (fraction), rest ASSERT-guarded |
| 4 | `core.uninitialized.UndefReturn` | Via header inlines -- marginal |
| 1 | `core.NullDereference` | CvBitfield.h -- marginal |
| 1 | `cplusplus.NewDelete` | LinkedList.h -- needs context investigation |
| ~60 | `deadcode.DeadStores` | Code quality, not crash risk |

### False Positives in `grep "error:"`
Lines containing `error:` in the logs are **not actual errors** -- they are `>>>` reference lines inside `lld-link: warning:` messages, showing which destructor sites reference the undefined sized `operator delete`. Both builds link successfully.
