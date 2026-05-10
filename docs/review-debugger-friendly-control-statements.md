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

**Note on project conventions**: Per project coding notes, many of these guard clauses should instead be using `PRECONDITION` (for bounds/input validation that could corrupt state) or `ASSERT` (for invariants). Converting single-line guard clauses to use these macros would simultaneously fix both the formatting issue and align with the project's assertion policy. Example:

```cpp
// Even better — use PRECONDITION per project guidelines:
int CvDiplomacyAI::GetWarScore(PlayerTypes ePlayer) const
{
    PRECONDITION(ePlayer >= 0 && ePlayer < MAX_CIV_PLAYERS, 0);
    // ... rest of function
}
```

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

### Low Risk
- All changes are purely **whitespace/formatting** — zero functional impact
- No binary output changes (compiler generates identical code)
- No impact on save compatibility
- C++03/TR1 compliance is unaffected
- VS2008 SP1 / v90 toolset compatibility is unaffected

### Moderate Concerns
- **Diff size**: ~648 changes across 54 files produces a significant diff, which could complicate concurrent PR reviews
- **FirePlace headers**: Semi-vendored Firaxis code; formatting changes add noise if these files are later diffed against upstream
- **Merge conflicts**: Large formatting PRs can conflict with in-flight feature branches

### Mitigations
- Changes are mechanical and can be easily verified with the detection script
- Phased rollout reduces per-PR diff size
- The 99.1% existing compliance means this is a small cleanup, not a codebase-wide reformat

## Implementation Recommendation

### Phase 0: Adopt the Rule for New Code (Immediate)

Add the rule to the project's coding standards documentation. All new and modified code should follow the rule going forward. This alone prevents the violation count from growing.

### Phase 1: Fix CvGameCoreDLL_Expansion2 (472 violations, one PR)

This is the actively maintained directory. Highest debugging value.

**Priority files** (highest violation counts, most actively debugged):
1. `CvMinorCivAI.cpp` — 114 violations
2. `CvPlayer.cpp` — 51 violations
3. `CvInfos.cpp` — 45 violations
4. `CvVotingClasses.cpp` — 45 violations
5. `CvDiplomacyAI.cpp` — 33 violations

**Approach**: Many of these guard clauses (particularly in CvMinorCivAI.cpp and CvDiplomacyAI.cpp) are bounds-validation patterns that could be replaced with `PRECONDITION` macros per the project's coding notes, addressing both the formatting rule and the assertion policy in a single pass.

### Phase 2: Fix FirePlace / CvWorldBuilderMap / CvGameCoreDLLUtil (176 violations)

Lower priority. These are less frequently debugged. Can be done in a separate PR.

### Automated Enforcement

The detection script used for this analysis (`find_violations.py`) can be adapted for CI use. Options include:
- Running it as a pre-commit hook or CI check on changed files only
- A `.clang-tidy` check (`readability-braces-around-statements`) enforces a stricter variant of this rule (requiring braces on all control bodies)

## Conclusion

This rule is highly implementable for this codebase:
- **99.1% compliance already exists** — the codebase overwhelmingly follows this pattern
- Only **648 violations** across 54 files need fixing
- All changes are **zero-risk formatting** with no functional impact
- The dominant violation pattern (guard clauses) has a **natural synergy** with the project's existing PRECONDITION/ASSERT conventions
- A phased approach minimizes merge conflict risk

**Recommendation**: Adopt the rule immediately for new code, then fix existing violations in 1-2 focused formatting PRs.
