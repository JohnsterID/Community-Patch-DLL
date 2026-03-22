# SDK Shim Analysis for Modern Compiler Support

## Summary

This document analyzes the feasibility of creating a "civ5-sdk-shim.dll" to encapsulate VC90 ABI dependencies, allowing the main GameCore DLL to be built with modern compilers.

## Architecture Overview

Understanding where ABI boundaries exist is critical:

```
┌─────────────────────────────────────────────────────────────────┐
│                      Game EXE (VC90)                            │
│  - Implements ICvEngineUtility (DLL calls into Game)            │
│  - Calls our interfaces via vtable (ICvPreGame, ICvGame, etc.)  │
└─────────────────────────────────────────────────────────────────┘
                              │
                    DLL Interface Boundary
                   (std types cross here!)
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Our GameCore DLL                             │
│  - Implements ICvPreGame, ICvGame, ICvUnit, etc.                │
│  - Calls SDK libraries internally                               │
│  - Calls back to Game via ICvEngineUtility                      │
└─────────────────────────────────────────────────────────────────┘
                              │
                    SDK Library Boundary
                    (shim proposal targets this)
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    SDK Libraries (VC90)                         │
│  FireWorks, FLua, CvGameDatabase, CvLocalization, etc.          │
└─────────────────────────────────────────────────────────────────┘
```

## Binary Analysis Confirmation

From decompiled `Civ5XP.c` (Linux binary with symbols), we confirm:

### std::vector<bool> Usage in Game Binary
```c
// Lines 9410-9413, 21833: Game implements std::vector<bool> operations
void __cdecl std::vector<bool>::reserve(void **a1, unsigned int a2);
void __cdecl std::vector<bool>::resize(struct type_info *a1, unsigned int a2, char a3);
int __cdecl std::vector<bool>::push_back(struct type_info *a1, _BYTE *a2);
```

The game binary creates and manipulates `std::vector<bool>` objects that cross the DLL boundary.

### std::vector<std::pair<std::string, std::string>> in Game
```c
// Line 864403-864451: Game implements these sync functions
void __cdecl CvDLLUtility::sendUnitSyncCheck(int a1, int a2, int a3, FMemoryStream *a4, int *a5)
{
  // ...
  std::__vector_base<std::pair<std::string,std::string>>::~__vector_base(&v7);
  // ...
}
```

The game's `ICvEngineUtility` implementation (which OUR DLL calls) expects `std::vector<std::pair<...>>` in VC90 ABI format.

## The Two Boundary Problem

### Boundary 1: Game EXE → Our DLL (CANNOT be shimmed)

The Game EXE is already compiled. It calls our interfaces like:
```cpp
// ICvPreGame1 - WE implement, GAME calls
virtual void DLLCALL setVictories(const std::vector<bool> & v) = 0;
```

The Game passes `std::vector<bool>` TO us. We cannot change what the Game sends.

### Boundary 2: Our DLL → SDK Libraries (CAN be shimmed)

This is what the issue proposes shimming:
```cpp
// SDK types like FLua::Table, FDataStream, FFastVector
// These are internal to our DLL's usage
```

## Key Questions Answered

### Q: Can std::vector<bool> in interfaces be removed with a shim?

**NO for Game→DLL interface.** The Game EXE is already compiled and calls `setVictories(std::vector<bool>&)`. We must accept this type in VC90 ABI format.

**The shim proposal only affects the DLL→SDK boundary, not the Game→DLL boundary.**

### Q: Do we still need VC90-compatible headers for interface code?

**YES, CONFIRMED.** Both interface directions use std types:

| Direction | Example | Who Creates the std Type |
|-----------|---------|--------------------------|
| Game→DLL | `setVictories(std::vector<bool>&)` | Game creates, passes to us |
| DLL→Game | `sendUnitSyncCheck(..., std::vector<pair>&)` | We create, pass to Game |

Since the Game EXE expects/provides VC90 ABI std types, our interface code MUST be VC90-compatible.

### Q: Must we shim all std types in interfaces?

**This is the wrong framing.** The real constraint is:

1. **Game↔DLL interfaces**: Cannot be shimmed (Game is already compiled)
2. **DLL↔SDK libraries**: Can be shimmed, but SDK uses Firaxis types (FFastVector, FDataStream) not std types

The SDK libraries primarily use Firaxis-defined types (`FFastVector<T>`, `FDataStream`, `FLua::Table`), not standard library types.

## Cross-Platform Binary Confirmation

All three game binaries exhibit the same interface architecture:

| Binary | DllGetGameContext | vector<bool> | NetSyncCheck |
|--------|-------------------|--------------|--------------|
| Civ5XP.c (Linux) | Named symbols | Lines 9410-21833 | Lines 864403-864451 |
| CivilizationV.exe.c (DX9) | Line 524689 | Line 442971 | Lines 730030, 732915 |
| CivilizationV_DX11.exe.c (DX11) | Line 497565 | Line 455099 | Lines 680004, 816492 |

All platforms:
- Load our DLL via `DllGetGameContext`
- Use `std::vector<bool>` (confirmed by "vector<bool> too long" error string)
- Implement `NetUnitSyncCheck`, `NetPlayerSyncCheck`, `NetCitySyncCheck` with std containers

## Why SDK Shimming Provides NO Practical Benefit

**Key insight: If we must build with VC90 for Game interface compatibility, shimming SDK libraries is pointless.**

### The Logic Chain

1. **Game EXE requires VC90 ABI** at the interface boundary
2. **Our interface implementations** (CvDllPreGame, CvDllGame, etc.) must be VC90
3. **Interface implementations USE SDK types** (FDataStream, FLua::Table, etc.)
4. **SDK types are already VC90-compatible** (same compiler)
5. **No shimming needed** - everything is already the same ABI

### The Hypothetical "Split Architecture" Problem

Even if we tried to split:
```
Interface Layer (VC90) ↔ Internal Logic (Modern) ↔ SDK Shim (VC90)
```

We'd need to shim BOTH boundaries:
- Interface Layer ↔ Internal Logic (VC90 ↔ Modern ABI mismatch)
- Internal Logic ↔ SDK Shim (Modern ↔ VC90 ABI mismatch)

This doubles the shimming work while providing no isolation benefit.

### Code Permeation

The interface code isn't a thin layer - it permeates throughout:
- `CvGame` uses `FDataStream` for serialization
- `CvUnit` uses `gDLL->` for engine callbacks  
- AI code calls `GC.GetEngineUserInterface()`
- Nearly ALL code paths touch Game interfaces eventually

There is no clean separation between "interface code" and "internal logic."

## Conclusion

**The shim approach provides NO practical benefit.**

| Question | Answer |
|----------|--------|
| Can shimming SDK libs help? | **No** - we still need VC90 for interfaces |
| Can we isolate modern code? | **No** - interfaces permeate everywhere |
| What's the actual constraint? | **Game EXE ABI** - cannot be changed |
| Is there any workaround? | **No** - Game is already compiled |

### The Real Answer

The proposal asks: "if we had a civ5-sdk-shim.dll (vc90) which exports the relevant symbols, then a (modern) gamecore could link against that dynamically"

**Answer: No, because:**
1. The Game EXE (not just SDK libs) requires VC90 ABI compatibility
2. The Game passes std types TO us and expects std types FROM us
3. Even with shimmed SDK libs, our interface code must be VC90
4. Since interface code uses SDK types anyway, shimming adds nothing

The VC90 requirement comes from the **Game EXE interface boundary**, not from the SDK libraries. The SDK libraries are just along for the ride.

## Appendix: Runtime Library Constraint (MD vs MDd)

### The Constraint

SDK libraries are compiled with `/MD` (Release CRT) only - no `/MDd` (Debug CRT) versions exist:

```
FirePlace/lib/FireWorksWin32.lib → "Final Release" build only
CvGameCoreDLLUtilWin32.lib → "Final Release" build only
```

### Why This Forces /MD for Debug Builds

From `VoxPopuli.vcxproj`:
```xml
<!-- Debug config uses /MD, NOT /MDd -->
<RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>  <!-- Line 77 -->
```

**Mixing runtimes would cause crashes:**
- `/MDd` uses debug heap (MSVCR90D.dll)
- `/MD` uses release heap (MSVCR90.dll)
- Memory allocated by SDK (via MD) freed by our code (via MDd) = **CRASH**
- Cross-heap operations are undefined behavior

### What MDd Would Provide (If We Could Use It)

| Feature | Description | Impact |
|---------|-------------|--------|
| Debug heap | Buffer overrun detection, use-after-free | Would catch memory bugs |
| `_CrtDbg*` | Memory leak tracking, heap validation | Would help find leaks |
| Iterator debugging | STL iterator validation | Would catch invalid iterators |
| `_ASSERTE` | CRT assertions with abort | Would catch CRT violations |

### Why We Don't Actually Miss Much

**Iterator debugging is explicitly disabled anyway:**
```cpp
// From TContainer.h:14-18
#define _SECURE_SCL 0
#define _HAS_ITERATOR_DEBUGGING 0
```

**We have alternative debug features:**
- Full debug symbols (`/Zi`)
- Disabled optimization (`/Od` in Debug)
- Custom `VPDEBUG` macro for conditional code
- PDB files for VS debugger attachment
- Logging via config.ini
- VS Diagnostic Tools for memory/CPU profiling (documented in DEVELOPMENT.md)

**CRT debug features wouldn't help anyway:**
- SDK libs would still allocate via release heap
- Only our allocations would use debug heap
- Partial debugging is worse than none (false sense of security)

### Conclusion

**Is /MD a limitation?** Yes, technically.

**Do we miss anything important?** Not really:
1. Iterator debugging is disabled regardless
2. CRT heap debugging can't work with mixed runtimes
3. We have compensating debug tools
4. Using /MD consistently is actually correct

The SDK being Release-only forces this, but it's not a significant debugging hindrance in practice.

## Appendix: Runtime Hooking Analysis

**Question:** Could runtime hooks intercept and translate ABI at the interface boundary?

The DLL already has runtime patching capability (see `VirtualProtect` usage in `CvDllGame.cpp` for `s_wantForceResync`). This section analyzes whether that approach could enable modern compiler usage.

### Interface Methods Using std Types

**Game→DLL Direction (ICvPreGame1)** - Game creates the object, passes TO us:

| Method | Type | Hookable? |
|--------|------|-----------|
| `setVictories(std::vector<bool>&)` | Game creates VC90 vector | ⚠️ Requires EXE patching |
| `setVersionString(std::string&)` | Game creates VC90 string | ⚠️ Requires EXE patching |
| `versionString()` returns `std::string` | We return to Game | ⚠️ Requires EXE patching |

**DLL→Game Direction (ICvDLLUtility)** - We create the object, pass TO Game:

| Method | Type | Hookable? |
|--------|------|-----------|
| `netMessageDebugLog(std::string&)` | We create | ✅ gDLL wrapper feasible |
| `sendUnitSyncCheck(..., vector<pair<string,string>>&)` | We create | ✅ gDLL wrapper feasible |
| `sendPlotSyncCheck(...)` | We create | ✅ gDLL wrapper feasible |
| `sendCitySyncCheck(...)` | We create | ✅ gDLL wrapper feasible |
| `sendPlayerSyncCheck(...)` | We create | ✅ gDLL wrapper feasible |
| `NetMessageDebug(std::string&)` | We create | ✅ gDLL wrapper feasible |

### The Two Different Hooking Approaches

**DLL→Game (6 methods):** Relatively straightforward
- Replace `gDLL` pointer with a wrapper object
- Wrapper translates modern std types → VC90 ABI layout before calling real Game function
- Contained entirely within our DLL code

**Game→DLL (3 methods):** Requires binary patching
- Game binary is already compiled
- When Game calls `pPreGame->setVictories(vec)`, it has already created a VC90 `std::vector<bool>` on its stack
- Must **patch the Game EXE at every call site** to:
  1. Jump to a trampoline function
  2. Read the VC90 struct layout from stack/registers
  3. Convert to modern ABI layout
  4. Call the real implementation
  5. Convert return values back if needed
  6. Return to Game

### Required Components

```
Per-Binary Address Tables (DX9, DX11, Tablet):
├── setVictories call sites (must find ALL in each binary)
├── setVersionString call sites
├── versionString call sites
└── vtable slot offsets for validation

ABI Translation Code:
├── VC90 std::string ↔ Modern std::string
│   - VC90 MSVC uses COW (copy-on-write) with 16-byte SSO buffer
│   - Modern MSVC uses different SSO threshold and no COW
│   - Must handle: _Bx union, _Mysize, _Myres, _Alval
│
├── VC90 std::vector<bool> ↔ Modern std::vector<bool>
│   - Both are bit-packed but internal pointers differ
│   - VC90: _Myvec (compressed), _Mysize
│   - Must handle bit offset calculations
│
└── VC90 std::vector<std::pair<std::string,std::string>> ↔ Modern
    - Combines both string and vector issues
    - Nested pair layout must also match
```

### Effort Estimate

| Task | Effort | Risk |
|------|--------|------|
| Find all 3 call sites in 3 binaries | 2-3 days | Addresses shift between versions |
| Write VC90↔Modern std::string translator | 1 week | COW semantics, SSO differences, edge cases |
| Write VC90↔Modern vector<bool> translator | 3-4 days | Bit-packing arithmetic |
| Write x86 trampoline hooks | 3-4 days | Calling convention, stack alignment |
| Write gDLL wrapper for 6 methods | 2-3 days | Straightforward |
| Integration and testing | 1 week | Any bug = game crash |
| **Total** | **~4 weeks** | **Ongoing maintenance burden** |

### Why This Approach Is Not Recommended

1. **Complexity vs. Benefit**: The current Clang + VC90 ABI build works. This adds significant complexity for marginal benefit.

2. **Fragility**: Runtime binary patching is inherently fragile. Any mistake in address calculation, ABI translation, or calling convention causes immediate crashes.

3. **Three Binaries**: Must maintain separate patch addresses for CivilizationV.exe (DX9), CivilizationV_DX11.exe, and CivilizationV_Tablet.exe.

4. **Limited Gain**: Even with perfect hooking, you still can't use many modern C++ features because the SDK headers must remain parseable by VC90-compatible compilers.

5. **Game is EOL**: Civilization V is end-of-life. The binaries won't change, but neither is there strong motivation to modernize the toolchain for a completed game.

### Clarification on "Two Shims"

The suggestion that "we need two shims" misunderstands the problem:

- A **shim DLL** can only intercept at link time (DLL→SDK boundary)
- The **Game→DLL boundary** cannot be shimmed by linking—it requires **runtime binary patching** of the game executable
- These are fundamentally different techniques with different complexity levels

### Conclusion

Runtime hooking is **technically feasible** but **not recommended**:

| Aspect | Assessment |
|--------|------------|
| Technical feasibility | Yes, with significant effort |
| Development time | ~4 weeks |
| Risk level | High (crashes on any bug) |
| Maintenance burden | Ongoing (3 binaries) |
| Practical benefit | Minimal (Clang build works today) |

The current VC90 + Clang setup remains the pragmatic choice.

## References

- [DEVELOPMENT.md](../DEVELOPMENT.md) - Build instructions mentioning VC90 requirement
- [build_vp_clang.py](../build_vp_clang.py) - Existing Clang-based build script
- [CvDllGame.cpp](../CvGameCoreDLL_Expansion2/CvDllGame.cpp) - Existing VirtualProtect hooking example
- Binary analysis from Civ5XP.c, CivilizationV.exe.c, CivilizationV_DX11.exe.c
