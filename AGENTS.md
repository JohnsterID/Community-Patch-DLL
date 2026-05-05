# Community-Patch-DLL — Agent Memory

## Authorship

All commits must use:
- Author and Committer: `JohnsterID <69278611+JohnsterID@users.noreply.github.com>`
- No `Co-authored-by:` lines
- Set with: `git config user.name "JohnsterID" && git config user.email "69278611+JohnsterID@users.noreply.github.com"`

## Repository Overview

Vox Populi (VP) mod for Civilization V. The mod consists of a single DLL:
`CvGameCore_Expansion2.dll`, built with the **VS2008 SP1 v90 toolset** (C++03/TR1).

Key constraints (from `Community-Patch-DLL-notes.txt`):
- Use `std::tr1::unordered_map`, not `std::unordered_map`
- No `auto`, no lambdas, no `dynamic_cast`
- MSVC v90 toolset + optional Clang cross-build (`build_vp_clang.py`)
- 32-bit x86 only; maintain binary save compatibility

## Code Style and Assert Usage

See `Community-Patch-DLL-notes.txt` for full detail.  Summary:
- **`ASSERT`** — invariant violations, logic bugs, developer mistakes; shows dialog, allows continue
- **`PRECONDITION`** — input validation, corrupted state, bounds checks; always crashes via `BUILTIN_TRAP()`
- **Null check** — only when null is a valid, expected, recoverable state

## Branch: `asan`

**Purpose:** ASAN/UBSan investigation branch.  ASAN is a dead end (see below).
UBSan works and is the active sanitizer on this branch.

**Cherry-pick candidates to `master`** — all real bugs confirmed by ASAN/UBSan:
- `CvAStar.cpp` — unsigned overflow in negative-index loop (`i += (int)vPlots.size()`)
- `CvDealClasses.h` — implicit sign-change on 5 int→uint getter returns
- `CvPlayer.cpp` — signed overflow guard for `iDomainValue *= 2`
- `CvPlot.cpp` — null-guard `GC.getGamePointer()` in constructor (static-init before CvGlobals)
- `CvPlot.cpp` — explicit `static_cast<uint>(-1)` for all-layers sentinel in `RemoveUnit`
- `CvPlot.h` — `1u<<` in `PlotBoolField` (was signed shift UB)
- `CvTacticalAI.cpp` — clamp `INT_MAX` danger score in `EM_FINAL`; eliminate dangling ref `bRef`
- `CvTacticalAI.h` — `m_iAttackPriority` short→int; `SComboMove::operator==` guards `getB()` behind `hasB()`
- `CvUnit.cpp` — `0xFFFFFFFFu` sentinels in `ClearPathCache`
- `CvLuaEnums.cpp` — `(int)FStringHash(...)` sign-change cast
- `CvPreGame.cpp` — check `_pos != string::npos` before arithmetic in `setNickname`
- `FFireTypes.h` — `MIN_INT` macro from unsigned `0x80000000` to `(-2147483647 - 1)`

## DLL-only ASAN: Closed Investigation

**Result: cannot work.  See `ASAN_INVESTIGATION.md` for full details.**

Short summary of why:

1. **Partial shadow mapping** — the pre-built `clang_rt.asan_dynamic-i386.dll` has
   compile-time fixed shadow offset 0x30000000.  In practice, after the VirtualQuery
   IAT hook releases the pre-reserved shadow range, a residual race window allows the
   CRT/loader to grab `[0x30000000, 0x3A000000)` before ASAN's `NtAllocateVirtualMemory`
   runs.  ASAN maps only `[0x3A000000, 0x50000000)`.  This covers only addresses
   ≥ 0x50000000.  Stack (~0x0012xxxx), CRT heap, and game EXE all live below 0x50000000;
   their shadow pages are `MEM_RESERVE` but not committed.  The first instrumented
   instruction touching a stack variable faults → `ERROR_DLL_INIT_FAILED (1114)`.

2. **No fixable hook point** — committing `[0x30000000, 0x3A000000)` before ASAN's
   conflict scan causes ASAN to abort; doing it after requires a hook point between
   ASAN init and first instrumented code that does not exist in the pre-built binary.

3. **Architectural mismatches even if shadow were fixed:**
   - `FNEW` uses `_malloc_dbg` — invisible to ASAN's heap interceptors (~95% of allocations)
   - EXE/DLL boundary pointers are untracked (EXE is not instrumented)
   - `CvDllGameContext` uses a private `HeapCreate` heap — bypasses ASAN interceptors
   - 71 MB instrumented DLL vs 7 MB vanilla — ~2× CPU/memory overhead

**Use UBSan instead** (`build_vp_clang.py --sanitizer ubsan`).  No shadow memory,
no allocator interception, correct DLL-only operation.

## Build System

- Visual Studio solution: `CvGameCoreDLL.vs2010.sln` (v90 toolset)
- Clang cross-build: `build_vp_clang.py` (supports `--sanitizer ubsan|asan`)
- v90 dependency zip: `/workspace/project/v90-dependencies.zip`

## IDA Databases

- `/workspace/project/CivilizationV.exe.i64` — main game EXE (IDA base 0x400000)
- `/workspace/project/Civ5XP.i64` — XP version reference (naming preserved)
- `Civ5XP.c` IDA address = `0x00400000 + RVA`
- See `/workspace/project/MCP.txt` for IDA MCP server setup
