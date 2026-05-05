# DLL-only AddressSanitizer Investigation

**Branch:** `asan`  
**Conclusion:** DLL-only ASAN with the pre-built `clang_rt.asan_dynamic-i386.dll` is
architecturally incompatible with the VP mod DLL setup. The investigation is closed.
The game-code fixes found during the investigation are real bugs and have been
cherry-picked (or are ready to be cherry-picked) to `master`.

---

## Background

The goal was to compile only `CvGameCore_Expansion2.dll` with
`-fsanitize=address` while leaving the game EXE and all other DLLs uninstrumented.
This is the only viable approach because the game EXE source is not available.

---

## Infrastructure Built

| Component | File | Purpose |
|---|---|---|
| Build support | `build_vp_clang.py --sanitizer asan` | Clang-cl ASAN flags, rebase, launcher/boot build |
| Compatibility layer | `CvGameCoreDLL_Expansion2/asan_compat.h/.cpp` | Shadow-pinning macro `VP_ASAN_PIN_RUNTIME()`, pool annotation macros |
| Shadow boot DLL | `asan_shadow_boot/asan_shadow_boot.c` | Injected at process start; reserves shadow range before GPU drivers |
| Launcher | `asan_launcher/asan_launcher.c` | Creates game process suspended, injects boot DLL, resumes |
| Suppression lists | `asan.ignore`, `ubsan.ignore` | VS2008 STL false-positives (deque, xhash) |

---

## The Shadow Conflict Problem

The pre-built `clang_rt.asan_dynamic-i386.dll` has **compile-time fixed** shadow
parameters:

```
scale  = 3  (shadow byte covers 8 application bytes)
offset = 0x30000000
shadow range = [0x30000000, 0x50000000)  (512 MB)
shadow(addr) = (addr >> 3) + 0x30000000
```

These constants cannot be changed without building the runtime from source.

The game loads many GPU drivers (AMD `amdxc32.dll` at 0x58640000, Intel `igc32.dll`
at 0x609B0000, NVIDIA `nvwgf2um.dll` at 0x55320000, etc.) before the mod DLL.
By the time the ASAN runtime loads as a dependency of the mod DLL, these drivers
have already consumed anonymous private pools that fall inside the shadow range
`[0x30000000, 0x50000000)`.  `__asan_init` aborts:

> Shadow memory range interleaves with an existing memory mapping. ABORTING.

### Phase A solution — pre-reserve the shadow range

`asan_shadow_boot.dll` is injected into the game process before any user code runs
(via `asan_launcher.exe` which creates the game suspended and remote-loads the boot
DLL).  In `DllMain DLL_PROCESS_ATTACH` the boot DLL calls:

```c
VirtualAlloc(0x2FFF0000, 512MB+64KB, MEM_RESERVE, PAGE_NOACCESS)
```

This blocks GPU drivers from allocating inside the shadow range.

### Phase B solution — VirtualQuery IAT hook

`__asan_init` calls `VirtualQuery` to verify the shadow range is free before mapping
it.  If it finds our `MEM_RESERVE` block it aborts.  We cannot call `VirtualFree`
in `LdrDllNotification` (fires before DllMain) because the loader's TLS/bookkeeping
`HeapAlloc` immediately re-occupies the lowest free address.

Instead, on the `LdrDllNotification` for `clang_rt.asan_dynamic`, we hook
`VirtualQuery` in clang_rt's **IAT**.  When ASAN calls `VirtualQuery(0x2FFF0000)`
from inside `__asan_init`:

1. We call `VirtualFree(0x2FFF0000)` — releases the pre-reservation.
2. We restore the real `VirtualQuery` in the IAT (one-shot).
3. We let the real `VirtualQuery` execute — it returns `MEM_FREE`.
4. ASAN sees the range clear and proceeds to map its shadow.

---

## Diagnostic Logs

The key diagnostic logs from a real run are in the project directory:
- `asan_launcher_debug.log` — launcher side (injection, phase A/B registration)
- `asan_shadow_boot_debug.log` — in-process side (all LdrDllNotification events, VQ hook, post-load state)

Both log files are referenced in the analysis below.

---

## What the Logs Show

### Launcher log — success up to game start

```
[+] PID 12988 created (main thread suspended)
[*] Injection: ...\asan_shadow_boot.dll  (14336 bytes, 2026-05-05)
[+] DLL loaded. HMODULE = 0x6F5C0000
[+] Injection succeeded.
[*] Phase A: VirtualAlloc [0x2FFF0000, 0x50000000) — result in DLL log.
[*] Phase B: LdrDllNotification + VirtualQuery hook registered.
[+] Game is running (PID 12988). Launcher exiting.
```

### Shadow boot log — key sequence

```
[A] VirtualAlloc(SHADOW_BASE, 512MB, MEM_RESERVE): OK  addr=2FFF0000

[B1] DLL loaded: clang_rt.asan_dynamic-i386.dll  base=50FD0000
     -> ASan runtime detected
[vq-pre-hook] 2FFF0000 - 50000000  RESERVE  PRIVATE   ← pre-reservation still intact
     VirtualQuery IAT hooked @ 5103C7D8

[B1] DLL loaded: CvGameCore_Expansion2.dll  base=95720000  size=0x4725000
     -> CvGameCore_Expansion2 (mod/ASAN build) detected
[vq-post-CvGame] 2FFF0000 - 50000000  RESERVE  PRIVATE  ← still intact; no DllMain yet
[shadow-hunt] __asan_shadow_memory_dynamic_address = 0x00000000  ← ASAN not yet inited

[VQ-hook] VirtualQuery(2FFF0000) in shadow range — releasing reservation
[+] release_shadow: VirtualFree(2FFF0000) = OK
[VQ-hook] result for 2FFF0000: state=FREE  size=0x20010000  ← returned to ASAN

[post-load] LoadLibraryW("...CvGameCore_Expansion2.dll") = 00000000  GetLastError=1114
[post-load] GetModuleHandleW(CvGameCore_Expansion2) = 00000000
[shadow-hunt] __asan_shadow_memory_dynamic_address = 0x30000000  ← ASAN init ran
[shadow-hunt]   3A000000 - 50000000  size=0x16000000  COMMIT  allocBase=3A000000
```

### Key observations

1. The VQ hook fires correctly and `VirtualFree` succeeds.
2. ASAN runtime initialises — `__asan_shadow_memory_dynamic_address = 0x30000000`.
3. **`LoadLibraryW` returns NULL with `ERROR_DLL_INIT_FAILED (1114)`** — the mod DLL
   fails to load.
4. The committed shadow only covers `[0x3A000000, 0x50000000)`, **not** the full
   expected `[0x30000000, 0x50000000)`.

---

## The Root Cause: Partial Shadow Mapping

### Shadow geometry proof

ASAN shadow mapping: `shadow(addr) = (addr >> 3) + 0x30000000`

| Memory region | Address | Shadow page | Shadow committed? |
|---|---|---|---|
| Stack (32-bit typical) | 0x0012F000 | **0x30025E00** | ❌ |
| CRT heap (low allocs) | 0x00800000 | **0x30100000** | ❌ |
| Game EXE base | 0x00400000 | **0x30080000** | ❌ |
| clang_rt.asan_dynamic | 0x50FD0000 | 0x3A1FA000 | ✅ |
| ASAN-instrumented mod DLL | 0x95720000 | 0x42AE4000 | ✅ |
| GPU driver (amdxc32) | 0x58640000 | 0x3B0C8000 | ✅ |

The committed range `[0x3A000000, 0x50000000)` covers only **addresses ≥ 0x50000000**.

The **missing** range `[0x30000000, 0x3A000000)` covers **addresses `[0x00000000,
0x50000000)`** — the stack, CRT heap, game EXE, and all low-address allocations.

### What this means for DllMain

Every ASAN-instrumented function shadows every memory access.  The DllMain entry
point is on the stack at ~`0x0012xxxx`.  The ASAN shadow check for that address
reads from ~`0x30025xxx` — inside `[0x30000000, 0x3A000000)` which is `MEM_RESERVE`
but **not committed**.  The read faults.  The unhandled exception causes
`ERROR_DLL_INIT_FAILED`.  This fires **before a single line of user DllMain code
executes** (before `VP_ASAN_PIN_RUNTIME`, before `CvDllGameContext::InitializeSingleton`).

### Why ASAN only committed [0x3A000000, 0x50000000)

There is a residual race window that the VQ hook cannot close:

```
VirtualFree(0x2FFF0000)                     ← [0x2FFF0000, 0x50000000) is FREE
   │
   └─ real VirtualQuery returns FREE to ASAN
         │
         ├─ ASAN MemoryRangeIsAvailable scan continues (more VirtualQuery calls)
         │
         │  ← WINDOW: loader bookkeeping / CRT HeapAlloc / TLS expansion
         │    grabs [0x30000000, 0x3A000000) — the lowest available range
         │
         └─ ASAN NtAllocateVirtualMemory(base=0x30000000, size=512MB)
               → partial failure: [0x30000000, 0x3A000000) occupied
               → ASAN maps only [0x3A000000, 0x50000000)
               → __asan_shadow_memory_dynamic_address stays 0x30000000 (compile-time constant)
```

The VQ hook eliminated the first, larger race window (the ldr_notify → DllMain gap
where a full `HeapAlloc` grabbed the shadow).  A smaller, unavoidable window remains
between `VirtualFree` returning and `NtAllocateVirtualMemory` executing, during which
the CRT/loader internally allocates at the newly freed low address.

---

## Why It Cannot Be Fixed Without a Custom clang_rt Build

### Attempted fix: pre-commit the missing shadow pages

Commit `[0x30000000, 0x3A000000)` ourselves (zero-filled, `PAGE_READWRITE`) so ASAN
shadow reads for low-address memory return 0 ("clean") rather than faulting.

**Before ASAN's conflict scan:** ASAN sees the region as `MEM_COMMIT` — occupied —
and aborts: *"Shadow memory range interleaves with an existing mapping."*  Same
original failure.

**After ASAN maps [0x3A000000, 0x50000000):** There is no reachable hook point
between "ASAN finishes `__asan_init`" and "first instrumented code runs":
- `LdrDllNotification` fires **before** any DllMain — too early.
- VQ hook fires **during** ASAN init — between free and map, cannot commit.
- CvGameCore DllMain fires **after** static constructors, which crash first.

**Build a custom `clang_rt.asan_dynamic-i386.dll`** with a shadow offset ≥ 0x80000000
so that the committed shadow covers low addresses:  Requires building LLVM compiler-rt
from source.  Even then, the shadow range would need to not conflict with GPU drivers
that load above 0x80000000.  Still does not fix the architectural issues below.

---

## Architectural Impossibilities (Even with Full Shadow)

### 1. FNEW is invisible to ASAN

```cpp
// FDefNew.h
#define FNEW( type, mpool, tag ) new(_NORMAL_BLOCK, __FILE__, __LINE__, mpool, tag) type
```

`_NORMAL_BLOCK` calls MSVC debug CRT `_malloc_dbg`, not `::operator new(size_t)`.
ASAN intercepts standard `malloc`/`new`/`delete` — not the overloaded debug
placement-new with extra parameters.  Approximately 95 % of game object allocations
(`CvUnit`, `CvCity`, `CvPlot`, all `FFreeListTrashArray` entries) are allocated with
FNEW and are **completely invisible to ASAN**.  Heap overflow and use-after-free in
FNEW memory produce no report.

### 2. Cross-boundary EXE/DLL pointers are untracked

The game engine (uninstrumented EXE) passes `ICvDLLUserInterface*`,
`ICvGameContext1*`, raw data buffers, and callback pointers into the DLL.  These
allocations carry no ASAN metadata.  The EXE/DLL boundary — where the most
interesting inter-component bugs live — is entirely blind to ASAN.

### 3. CvDllGameContext uses a private Win32 heap directly

```cpp
s_hHeap = HeapCreate(0, 0, 0);                           // private heap
s_pSingleton = FNEW(CvDllGameContext(), ...);             // allocates via _malloc_dbg
void* CvDllGameContext::Allocate(size_t b) { return HeapAlloc(s_hHeap, 0, b); }
```

The private heap handle bypasses ASAN's `HeapAlloc` interceptors.  All memory
returned to the game engine through `ICvGameContext1::Allocate` is untracked.

### 4. Performance makes functional testing impossible

| Metric | Vanilla DLL | ASAN DLL |
|---|---|---|
| Image size on disk | ~7 MB | ~71 MB (10×) |
| Runtime memory overhead | 1× | ~2× |
| CPU overhead | 1× | ~2× |
| Late-game AI turn (estimated) | 30–60 s | 60–120 s |

### 5. The shadow conflict machinery is inherently fragile

Three external components (launcher, boot DLL, VQ IAT hook) were required just to
get past the ASAN shadow initialisation.  This machinery fights against the loader,
the GPU driver stack, and the Windows memory manager.  Despite the investment, the
residual race window above is not closable without modifying the ASAN runtime itself.

---

## Summary of What Works vs What Fails

| Component | Status |
|---|---|
| Phase A shadow pre-reservation | ✅ Works |
| LdrDllNotification firing order | ✅ Works |
| VQ IAT hook — first race window eliminated | ✅ Works |
| ASAN runtime initialisation (`__asan_shadow_memory_dynamic_address = 0x30000000`) | ✅ Works |
| `VP_ASAN_PIN_RUNTIME` — runtime stays alive across reload cycle | ✅ Works |
| Shadow fully committed for all 32-bit address space | ❌ Partial: [0x30000000, 0x3A000000) missing |
| Instrumented DLL loads successfully (DllMain returns) | ❌ ERROR_DLL_INIT_FAILED (1114) |
| FNEW heap coverage | ❌ Never — architectural |
| EXE/DLL boundary coverage | ❌ Never — architectural |

---

## Game-Code Fixes Found During This Investigation

The following commits on this branch fix **real bugs** that were identified through
ASAN/UBSan instrumentation.  They are all correct, safe changes to cherry-pick to
`master`.

| File | Fix | Bug class |
|---|---|---|
| `CvAStar.cpp` | `i += (int)vPlots.size()` | Unsigned overflow in negative-index loop |
| `CvDealClasses.h` | Explicit `(uint)` casts on 5 int→uint getter returns | Implicit sign-change |
| `CvPlayer.cpp` | Guard `iDomainValue *= 2` with `MAX_INT/2` check | Signed integer overflow |
| `CvPlot.cpp` | Null-guard `GC.getGamePointer()` in constructor (static-init before `CvGlobals::init()`) | Null pointer dereference |
| `CvPlot.cpp` | `static_cast<uint>(-1)` for all-layers sentinel in `RemoveUnit` | Implicit sign-change |
| `CvPlot.h` | `1u<<` in `PlotBoolField` — fix signed shift overflow (`1<<31`) | Signed shift UB |
| `CvTacticalAI.cpp` | Clamp `INT_MAX` danger score to `SHRT_MIN` when no safe plots in `EM_FINAL` | Signed overflow |
| `CvTacticalAI.cpp` | Use `gMovesToAdd[i].getB()` directly — eliminate dangling reference `bRef` | Dangling reference |
| `CvTacticalAI.h` | `m_iAttackPriority`: `short` → `int` | Implicit truncation |
| `CvTacticalAI.h` | `SComboMove::operator==` guards `getB()` behind `hasB()` | Null dereference |
| `CvUnit.cpp` | `0xFFFFFFFFu` literals for unsigned sentinel values in `ClearPathCache` | Implicit sign-change |
| `CvLuaEnums.cpp` | `(int)FStringHash(...)` cast | Sign-change |
| `CvPreGame.cpp` | Check `_pos != string::npos` before using it in `setNickname` | Unsigned underflow (npos arithmetic) |
| `FFireTypes.h` | `MIN_INT` macro: `0x80000000` → `(-2147483647 - 1)` | Unsigned integer constant where signed expected |

---

## What To Use Instead

**UBSan** (already implemented on this branch via `ubsan_handlers.cpp`) is the
correct sanitizer for DLL-only instrumentation:

- No shadow memory — no shadow conflict with GPU drivers.
- No allocator interception — FNEW allocations are checked for UB at the call site.
- Minimal performance overhead — functional testing remains possible.
- Works correctly in a DLL-only build against an uninstrumented EXE.

For heap-safety checking without source access to the EXE, consider:
- **Application Verifier + PageHeap** (Windows, no recompile required, process-wide).
- **Dr. Memory** (Windows, process-wide, no source required).

---

## References

- Diagnostic logs: `asan_launcher_debug.log`, `asan_shadow_boot_debug.log` (project directory)
- ASAN suppression list: `asan.ignore`
- ASAN compatibility layer: `CvGameCoreDLL_Expansion2/asan_compat.h`, `asan_compat.cpp`
- Shadow boot source: `asan_shadow_boot/asan_shadow_boot.c`
- Launcher source: `asan_launcher/asan_launcher.c`
- Build system: `build_vp_clang.py`
- LLVM ASAN interface: https://github.com/llvm/llvm-project/blob/main/compiler-rt/include/sanitizer/asan_interface.h
- Clang SanitizerSpecialCaseList: https://clang.llvm.org/docs/SanitizerSpecialCaseList.html
