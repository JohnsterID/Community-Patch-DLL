# Review: Debugger-Friendly Control Statement Formatting

## Rule Summary

> Always place if/else/for/while statement bodies on separate lines from the condition to allow precise debugger breakpoint placement.

This document reviews the feasibility and impact of enforcing this rule across the Community-Patch-DLL codebase.

## Current Codebase State

| Metric | Value |
|--------|-------|
| Total source files (.cpp/.h, excluding ThirdPartyLibs) | 464 |
| Total lines of code | ~631,000 |
| Control statements analyzed (if/for/while with balanced parens on one line) | 58,904 |
| Already conforming (body on separate line or braced) | 58,364 |
| Non-conforming (body on same line as condition) | ~648 |
| **Current compliance rate** | **99.1%** |

The codebase is already overwhelmingly compliant with this rule. Only ~648 instances across 54 files need attention.

## Violation Breakdown

### By Keyword

| Keyword | Count | % of Violations |
|---------|-------|-----------------|
| `if`    | 536   | 82.7%           |
| `else`  | 109   | 16.8%           |
| `while` | 2     | 0.3%            |
| `for`   | 1     | 0.2%            |

### By Body Pattern

| Pattern | Count | % of Violations | Example |
|---------|-------|-----------------|---------|
| Early return / guard clause | 411 | 63.4% | `if (ePlayer < 0) return 0;` |
| Other (compound ops, etc.) | 143 | 22.1% | `else m_byFlags &= ~eFlag;` |
| Function call | 31 | 4.8% | `if (m_ptr) m_ptr->Destroy();` |
| Compound assignment | 28 | 4.3% | `if (bVal) m_Mem[uiByte] |= byMask;` |
| Simple assignment | 25 | 3.9% | `if (bIsFree) m_iFreeGPExtra1Created++;` |
| Continue | 9 | 1.4% | `if (pEntry == NULL) continue;` |
| Break | 1 | 0.2% | `if (iDistance == 0) break;` |

### By Directory

| Directory | Violations | Notes |
|-----------|-----------|-------|
| CvGameCoreDLL_Expansion2 | 472 | Main game logic — actively maintained |
| FirePlace | 156 | Firaxis engine headers — modified but semi-vendored |
| CvWorldBuilderMap | 18 | Map builder — mostly Firaxis base code |
| CvGameCoreDLLUtil | 2 | Utility headers |

### Top 10 Files

| File | Violations |
|------|-----------|
| CvMinorCivAI.cpp | 114 |
| FLuaTypeExposure.h | 68 |
| CvPlayer.cpp | 51 |
| CvInfos.cpp | 45 |
| CvVotingClasses.cpp | 45 |
| CvDiplomacyAI.cpp | 33 |
| CvPlot.cpp | 23 |
| FLuaStaticFunctions.h | 23 |
| CvWorldBuilderMapLoader.cpp | 22 |
| CvSiteEvaluationClasses.cpp | 21 |

## Violation Patterns — Detailed Analysis

### Pattern 1: Guard Clauses (411 instances, 63%)

The dominant violation pattern. These are parameter-validation or early-exit checks at the top of functions:

```cpp
// Current (non-conforming):
int CvDiplomacyAI::GetWarScore(PlayerTypes ePlayer) const
{
    if (ePlayer < 0 || ePlayer >= MAX_CIV_PLAYERS) return 0;
    // ... rest of function
}

// Proposed (conforming):
int CvDiplomacyAI::GetWarScore(PlayerTypes ePlayer) const
{
    if (ePlayer < 0 || ePlayer >= MAX_CIV_PLAYERS)
        return 0;
    // ... rest of function
}
```

**Debugger benefit**: Allows setting a breakpoint specifically on the `return 0` to catch when invalid parameters are passed, without also breaking on every call to the function.

#### Guard Clauses vs. ASSERT / PRECONDITION — Important Distinctions

The original review suggested that guard clauses could be "upgraded" to PRECONDITION/ASSERT
macros. **This is incorrect for most cases.** The macros have fundamentally different
semantics from guard clauses and cannot be used as drop-in replacements.

**How the macros actually work:**

| Macro | On failure | Returns? | Alters control flow? | Always active? |
|-------|-----------|----------|---------------------|---------------|
| `ASSERT(expr, ...)` | Shows dialog, optionally breaks to debugger | No | No — execution continues | Only when CVASSERT_ENABLE is defined |
| `PRECONDITION(expr, ...)` | Shows dialog, then `BUILTIN_TRAP()` (crashes) | No | Yes — crashes the program | Always |
| Guard clause `if (bad) return val;` | Returns a safe default value | Yes | Yes — returns from function | Always |

**Key problem**: Neither ASSERT nor PRECONDITION produces a return value. A guard clause
like `if (ePlayer < 0) return 0;` returns a safe default to the caller. PRECONDITION
crashes the game. ASSERT does nothing to control flow — execution continues into the
function body with the invalid parameter.

**Existing dead code problem (112 instances):**

The codebase currently has 112 instances where PRECONDITION is followed by a guard clause
checking the same condition:

```cpp
// ACTUAL CODE in CvMinorCivAI.cpp — the guard clause is dead code:
int CvMinorCivAI::GetNumActiveQuestsForPlayer(PlayerTypes ePlayer) const
{
    PRECONDITION(ePlayer >= 0, "ePlayer is expected to be non-negative");
    PRECONDITION(ePlayer < MAX_MAJOR_CIVS, "ePlayer is expected to be within maximum bounds");
    if(ePlayer < 0 || ePlayer >= MAX_MAJOR_CIVS) return 0;  // ← UNREACHABLE
    return m_QuestsGiven[ePlayer].size();
}
```

PRECONDITION calls `BUILTIN_TRAP()` which crashes immediately. The `if` guard on the
next line can never execute — it is dead code that gives a false impression of graceful
error handling.

| Location | PRECONDITION + dead guard | Guard clause only |
|----------|--------------------------|-------------------|
| CvMinorCivAI.cpp | 71 | 42 |
| CvVotingClasses.cpp | 10 | 35 |
| CvPlot.cpp | 9 | 14 |
| CvCultureClasses.cpp | 4 | — |
| CvEspionageClasses.cpp | 4 | — |
| CvGlobals.cpp | 4 | — |
| CvUnit.cpp | 4 | — |
| Other files | 6 | 167+ |
| **Total** | **112** | **258** |

**Correct approach for each case:**

1. **PRECONDITION + dead guard clause (112 instances)** — Delete the dead guard clause.
   PRECONDITION already enforces the contract by crashing. The guard creates unreachable
   dead code. *(Note: this is a separate cleanup from the formatting rule.)*

2. **Guard-only, Lua-exposed function (many of the 258)** — **Keep the guard clause,
   just reformat.** Many functions like `GetWarScore`, `DoCompletedQuestsForPlayer`, etc.
   are called from Lua wrappers (e.g., `CvLuaPlayer.cpp`) that do NO input validation.
   The guard clause is the only protection against bad Lua arguments. Replacing it with
   PRECONDITION would crash the game on bad Lua input instead of returning a safe default.

   ```cpp
   // Lua wrapper does NO validation:
   int CvLuaPlayer::lGetWarScore(lua_State* L)
   {
       CvPlayer* pkPlayer = GetInstance(L);
       const PlayerTypes ePlayer = (PlayerTypes) lua_tointeger(L, 2);  // raw cast
       const int iResult = pkPlayer->GetDiplomacyAI()->GetWarScore(ePlayer);
       // ...
   }
   ```

3. **Guard-only, internal-only function, bad input = corrupted state** — Replace with
   PRECONDITION. These are the only cases where the macro is appropriate. The function
   is never called from Lua or external APIs, and invalid input means something has
   gone seriously wrong.

4. **Guard-only, internal function, bad input = developer mistake but recoverable** —
   Add ASSERT before the guard clause (do not replace it):

   ```cpp
   ASSERT(ePlayer >= 0 && ePlayer < MAX_CIV_PLAYERS, "Invalid player index");
   if (ePlayer < 0 || ePlayer >= MAX_CIV_PLAYERS)
       return 0;
   ```

   This gives debug visibility while preserving safe runtime behavior.

**Per project coding notes** (Community-Patch-DLL-notes.txt):
- PRECONDITION: "For input validation and parameter checking at function entry points —
  when the condition failure indicates corrupted state that cannot be recovered"
- ASSERT: "To enforce invariants and catch logic bugs — when a failure means a developer
  mistake, not user action — does NOT crash the game"
- Null check / guard: "Only when null is an expected, valid state — when interacting with
  dynamic systems (e.g., scripting, UI bindings) — when the code can gracefully handle the
  null case and continue execution"

**Bottom line**: The formatting rule (move body to separate line) should be applied
independently from any ASSERT/PRECONDITION conversion. They are separate concerns.
Reformatting is mechanical and safe. Macro conversion requires per-instance analysis
of call sites and failure semantics.

### Pattern 2: Conditional Assignments / Operations (53 instances)

```cpp
// Current:
if (bIsFree) m_iFreeGPExtra1Created++;
if( bValue ) m_byFlags |= eFlag;
else m_byFlags &= ~eFlag;

// Proposed:
if (bIsFree)
    m_iFreeGPExtra1Created++;
if (bValue)
    m_byFlags |= eFlag;
else
    m_byFlags &= ~eFlag;
```

**Debugger benefit**: Clearly separates the condition from the effect, making it possible to break on the assignment independently.

### Pattern 3: Conditional Function Calls (31 instances)

```cpp
// Current:
if (m_ptr) m_ptr->Destroy();
if (pkInstance) delete pkInstance;

// Proposed:
if (m_ptr)
    m_ptr->Destroy();
if (pkInstance)
    delete pkInstance;
```

### Pattern 4: FirePlace / FLua Patterns (156 instances)

Many are repeated patterns in Firaxis engine headers (e.g., error handling in `FLuaTypeExposure.h`):

```cpp
// Current:
else Error(_T("%s could not be called: Call cannot be made on NULL"), szLuaFnName);

// Proposed:
else
    Error(_T("%s could not be called: Call cannot be made on NULL"), szLuaFnName);
```

These are semi-vendored Firaxis headers that have been modified by the project. Changes here carry slightly higher risk but are still safe since they are formatting-only.

## Risk Assessment

### Low Risk (formatting changes only — Patterns 1-4)
- All formatting changes are purely **whitespace** — zero functional impact
- No binary output changes (compiler generates identical code)
- No impact on save compatibility
- C++03/TR1 compliance is unaffected
- VS2008 SP1 / v90 toolset compatibility is unaffected

### Dead Code Deletion (112 PRECONDITION + guard instances)
- Deleting unreachable guard clauses after PRECONDITION is functionally safe
  (the code can never execute), but changes the *apparent* contract of the function
- Should be reviewed per-file to confirm PRECONDITION is the intended behavior
  (not the guard clause)

### Moderate Concerns
- **Diff size**: ~648 formatting changes across 54 files produces a significant diff,
  which could complicate concurrent PR reviews
- **FirePlace headers**: Semi-vendored Firaxis code; formatting changes add noise if
  these files are later diffed against upstream
- **Merge conflicts**: Large formatting PRs can conflict with in-flight feature branches

### Mitigations
- Formatting changes are mechanical and can be easily verified with the detection script
- Phased rollout reduces per-PR diff size
- The 99.1% existing compliance means this is a small cleanup, not a codebase-wide reformat

## Implementation Recommendation

### Phase 0: Adopt the Rule for New Code (Immediate)

Add the rule to the project's coding standards documentation. All new and modified code should follow the rule going forward. This alone prevents the violation count from growing.

### Phase 1: Fix CvGameCoreDLL_Expansion2 Formatting (472 violations, one PR)

This is the actively maintained directory. Highest debugging value.
**This phase is formatting-only** — move bodies to separate lines, no semantic changes.

**Priority files** (highest violation counts, most actively debugged):
1. `CvMinorCivAI.cpp` — 114 violations
2. `CvPlayer.cpp` — 51 violations
3. `CvInfos.cpp` — 45 violations
4. `CvVotingClasses.cpp` — 45 violations
5. `CvDiplomacyAI.cpp` — 33 violations

### Phase 1b: Delete Dead Guard Clauses After PRECONDITION (112 instances, separate PR)

These are unreachable guard clauses that follow PRECONDITION checks on the same
condition. The guard clause can never execute because PRECONDITION already crashed.
Removing them eliminates dead code and the false impression of graceful error handling.

**This is a semantic-awareness change** — reviewers should confirm for each function
that PRECONDITION (crash) is the intended behavior, not the guard clause (return default).

### Phase 2: Fix FirePlace / CvWorldBuilderMap / CvGameCoreDLLUtil (176 violations)

Lower priority. These are less frequently debugged. Can be done in a separate PR.

### NOT Recommended: Bulk PRECONDITION/ASSERT Conversion of Guard Clauses

Converting the 258 guard-only clauses to PRECONDITION or ASSERT requires per-instance
analysis of whether the function is Lua-exposed, whether bad input is expected or a bug,
and what the appropriate failure mode is. This is a separate project from the formatting
rule and should not be combined with it.

### Automated Enforcement

The detection script used for this analysis can be adapted for CI use. Options include:
- Running it as a pre-commit hook or CI check on changed files only
- A `.clang-tidy` check (`readability-braces-around-statements`) enforces a stricter
  variant of this rule (requiring braces on all control bodies)

## Conclusion

This rule is highly implementable for this codebase:
- **99.1% compliance already exists** — the codebase overwhelmingly follows this pattern
- Only **648 violations** across 54 files need fixing
- Formatting changes are **zero-risk** with no functional impact
- A **separate finding**: 112 guard clauses are dead code behind PRECONDITION — these
  should be cleaned up independently
- Guard clause → PRECONDITION/ASSERT conversion is a **separate concern** requiring
  per-instance analysis and must not be conflated with the formatting rule
- A phased approach minimizes merge conflict risk

**Recommendation**: Adopt the rule immediately for new code, then fix existing formatting
violations in 1-2 focused PRs. Address dead PRECONDITION+guard code in a separate PR.
Keep ASSERT/PRECONDITION conversion as a distinct future effort with per-function review.
