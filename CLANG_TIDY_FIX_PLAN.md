# Clang-Tidy Corruption Fix Plan

**Problem:** Running all 14 checks produces `);` corruption in 41 files  
**Goal:** Identify and drop problematic checks OR fix script to handle corruption  
**Approach:** Incremental testing with check subsets

---

## Corruption Analysis

### What We Know

**Corruption Pattern:**
```cpp
// Before
getGameTurn()

// After (CORRUPTED)
getGameTurn());
```

**Affected:**
- 41 files with corruption
- Pattern: `);` added after function calls returning objects
- Examples: `getGameTurn()`, `GetCity()`, `getPopulation()`, `end()`, `plot()`, etc.

### What We DON'T Know

❌ Which specific check causes this corruption  
❌ Whether it's one check or multiple checks interacting  
❌ Whether the corruption is in clang-tidy or our script's handling

---

## Check Risk Assessment

### Tier 1: SAFE (Low Risk)

These checks make simple, localized changes:

1. `readability-isolate-declaration` ✅
   - Splits `int a, b;` → `int a; int b;`
   - Low risk: simple pattern, no complex logic

2. `modernize-use-bool-literals` ✅
   - Changes `0` → `false`, `1` → `true` in boolean contexts
   - Low risk: simple token replacement

3. `readability-container-size-empty` ✅
   - Changes `.size() == 0` → `.empty()`
   - Low risk: well-defined pattern

4. `readability-inconsistent-declaration-parameter-name` ✅
   - Renames function parameters to match declaration
   - Low risk: simple rename

### Tier 2: MEDIUM RISK

These checks modify code structure:

5. `cppcoreguidelines-init-variables` ⚠️
   - Adds initializers: `int x;` → `int x = 0;`
   - Medium risk: Has caused issues with va_list
   - **Note:** We already filter problematic patterns

6. `readability-string-compare` ⚠️
   - Simplifies string comparisons
   - Medium risk: might interact with function calls

### Tier 3: HIGH RISK (Likely Culprits)

These checks manipulate expressions and could cause `);` corruption:

7. `readability-simplify-boolean-expr` ⚠️⚠️
   - **SUSPECT #1:** Manipulates complex boolean logic
   - Could misidentify expression boundaries

8. `readability-avoid-return-with-void-value` ⚠️⚠️
   - **SUSPECT #2:** Manipulates return statements
   - Deals with function call expressions

9. `readability-redundant-declaration` ⚠️⚠️
   - Removes redundant forward declarations
   - Could miscalculate removal locations

10. `readability-redundant-function-ptr-dereference` ⚠️⚠️⚠️
    - **SUSPECT #3:** Manipulates function pointer syntax
    - Direct interaction with function calls

11. `readability-redundant-smartptr-get` ⚠️⚠️
    - Removes `.get()` calls on smart pointers
    - Could affect function call chains

12. `readability-redundant-string-cstr` ⚠️⚠️⚠️
    - **SUSPECT #4:** Removes `.c_str()` calls
    - Most likely to interact with `;` in expressions

13. `readability-redundant-string-init` ⚠️⚠️
    - Removes redundant string initialization
    - Could overlap with other changes

14. `readability-static-accessed-through-instance` ⚠️⚠️
    - Changes instance access to static access
    - Could affect function call syntax

---

## Solution Strategy

### Option A: Incremental Check Testing (Recommended)

Test checks in tiers to identify the problematic one(s):

```bash
# Test Tier 1 only (safe checks)
python3 run_clang_tidy_tiered.py --tier 1

# If successful, test Tier 1 + Tier 2
python3 run_clang_tidy_tiered.py --tier 2

# If successful, add checks from Tier 3 one by one
python3 run_clang_tidy_tiered.py --add readability-simplify-boolean-expr
python3 run_clang_tidy_tiered.py --add readability-avoid-return-with-void-value
# etc.
```

### Option B: Drop High-Risk Checks (Quick Fix)

Create a "safe mode" with only Tier 1 + Tier 2 checks (6 checks instead of 14):

```python
SAFE_CHECKS = [
    "readability-isolate-declaration",
    "modernize-use-bool-literals",
    "readability-container-size-empty",
    "readability-inconsistent-declaration-parameter-name",
    "cppcoreguidelines-init-variables",  # With existing filtering
    "readability-string-compare",
]
```

**Pros:** Fast, guaranteed to avoid corruption  
**Cons:** Misses potential improvements from 8 other checks

### Option C: Enhanced Corruption Detection (Fix Script)

Add specific pattern detection for `);` corruption:

```python
def detect_semicolon_paren_corruption(content):
    """Detect ); added after function calls"""
    # Pattern: identifier());
    if re.search(r'\w+\(\s*\)\s*\);', content):
        # Check if this is intentional (e.g., in a for loop) or corruption
        # ...
    return False
```

**Pros:** Keeps all checks, fixes at source  
**Cons:** Complex, might miss edge cases

---

## Recommended Action Plan

### Phase 1: Quick Win (30 minutes)

1. **Run safe subset** (Tier 1 only - 4 checks)
2. **Verify no corruption**
3. **Apply fixes if clean**
4. **Commit with "Safe subset only" note**

### Phase 2: Identify Culprits (2-3 hours)

1. **Add Tier 2 checks** (+ 2 checks = 6 total)
2. **Test for corruption**
3. **If clean, add Tier 3 checks one by one**
4. **Document which check(s) cause corruption**

### Phase 3: Decision

Based on Phase 2 findings:

**If 1-2 checks cause problems:**
- Drop those checks permanently
- Document in code and README
- Keep other 12-13 checks

**If 3+ checks cause problems:**
- Implement enhanced corruption detection (Option C)
- Or use safe subset permanently (Option B)

---

## Implementation

### Create Tiered Test Script

```python
# run_clang_tidy_tiered.py
TIER_1_CHECKS = [
    "readability-isolate-declaration",
    "modernize-use-bool-literals",
    "readability-container-size-empty",
    "readability-inconsistent-declaration-parameter-name",
]

TIER_2_CHECKS = TIER_1_CHECKS + [
    "cppcoreguidelines-init-variables",
    "readability-string-compare",
]

TIER_3_CHECKS = TIER_2_CHECKS + [
    "readability-simplify-boolean-expr",
    "readability-avoid-return-with-void-value",
    # Add one at a time for testing
]
```

### Test Protocol

For each tier:
1. Run clang-tidy with that tier's checks
2. Check for corruption in output
3. If clean: proceed to next tier
4. If corrupted: identify which new check caused it

---

## Expected Outcomes

### Best Case
- Identify 1-2 problematic checks
- Drop them
- Keep 12-13 working checks
- Document findings

### Worst Case  
- Multiple checks interact badly
- Use safe subset (6 checks)
- Still better than nothing

### Middle Case
- 8-10 checks work safely
- Drop 4-6 problematic ones
- Document for future reference

---

## Documentation Requirements

After testing, update:

1. **run_clang_tidy.py** - Final working check list
2. **ANALYSIS_TOOLS.md** - Document which checks are safe/unsafe
3. **CLANG_TIDY_RESULTS.md** - Test results and findings
4. **README/DEVELOPMENT.md** - Usage notes

---

## Timeline Estimate

- **Option A (Full testing):** 2-3 hours
- **Option B (Safe subset):** 30 minutes  
- **Option C (Fix script):** 4-6 hours + uncertain success

**Recommendation:** Start with Option B (quick win), then pursue Option A (identify culprits) if time permits.

---

**Created:** January 13, 2026  
**Status:** READY TO IMPLEMENT  
**Next Step:** Choose strategy and execute
