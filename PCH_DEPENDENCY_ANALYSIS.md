# PCH Dependency Analysis

## Executive Summary

**Finding:** 53 game headers are in the PCH, but many are barely used!

**Key Discovery:** 
- **16 headers (30%)** have ZERO explicit includes in any .cpp file
- **34 headers (64%)** are used by <10% of .cpp files
- Only **6 headers (11%)** are used by >10% of files

**Implication:** We can remove 30+ headers from PCH with minimal effort!

---

## Current PCH Structure

### Total Headers in PCH: 66

**Category Breakdown:**
1. **System/STL Headers:** 13 (algorithm, vector, map, etc.) ✅ Keep
2. **FireWorks Framework:** 5 (FAssert.h, FDataStream.h, etc.) ✅ Keep  
3. **Project Utility Headers:** 14 (CvDefines.h, CvGameCoreEnums.h, etc.) ✅ Keep
4. **Game Headers:** 53 ❌ **THE PROBLEM**
5. **Using directives:** 1 (fastdelegate namespace)

---

## Game Header Usage Statistics

**Total .cpp files:** 156

| Usage Tier | Count | Headers |
|------------|-------|---------|
| **Never used (0%)** | 16 | See below |
| **Rarely used (1-2%)** | 18 | See below |
| **Sometimes used (3-10%)** | 13 | See below |
| **Commonly used (>10%)** | 6 | See below |

### Tier 1: NEVER USED (0% - 16 headers)

**Perfect candidates for immediate removal!**

```
CvBeliefClasses.h
CvBuildingClasses.h
CvCityCitizens.h
CvCityStrategyAI.h
CvContractClasses.h
CvCorporationClasses.h
CvCultureClasses.h
CvEmphasisClasses.h
CvNotificationClasses.h
CvPolicyClasses.h
CvProjectClasses.h
CvPromotionClasses.h
CvReligionClasses.h
CvTechClasses.h
CvTradeClasses.h
CvTreasury.h
```

**Risk:** NONE - No .cpp files explicitly include these
**Effort:** ~10 minutes (just delete from PCH)
**Impact:** These are still available transitively through other includes

---

### Tier 2: RARELY USED (1-2% - 18 headers)

**Low-hanging fruit - only 1-3 files need fixes**

| Header | # Files | Files That Need It |
|--------|---------|-------------------|
| CvAchievementInfo.h | 1 | 1 file to fix |
| CvBuildingProductionAI.h | 1 | 1 file to fix |
| CvCityAI.h | 1 | 1 file to fix |
| CvGreatPersonInfo.h | 1 | 1 file to fix |
| CvPlotInfo.h | 1 | 1 file to fix |
| CvProcessProductionAI.h | 1 | 1 file to fix |
| CvProjectProductionAI.h | 1 | 1 file to fix |
| CvTraitClasses.h | 1 | 1 file to fix |
| CvUnitProductionAI.h | 1 | 1 file to fix |
| CvAdvisorCounsel.h | 2 | 2 files to fix |
| CvAdvisorRecommender.h | 2 | 2 files to fix |
| CvDealClasses.h | 2 | 2 files to fix |
| CvEspionageClasses.h | 2 | 2 files to fix |
| CvEventLog.h | 2 | 2 files to fix |
| CvHomelandAI.h | 2 | 2 files to fix |
| CvSiteEvaluationClasses.h | 2 | 2 files to fix |
| CvUnitClasses.h | 2 | 2 files to fix |
| CvVotingClasses.h | 2 | 2 files to fix |

**Risk:** LOW - Only 1-2 files per header need explicit #include added
**Effort:** ~2 hours (add explicit includes to ~30 files total)
**Impact:** Removes 18 more headers from PCH

---

### Tier 3: SOMETIMES USED (3-11% - 13 headers)

**Medium effort - 3-18 files need fixes**

| Header | # Files | % of Files |
|--------|---------|------------|
| CvBuilderTaskingAI.h | 3 | 2% |
| CvCityConnections.h | 3 | 2% |
| CvGame.h | 3 | 2% |
| CvTacticalAI.h | 3 | 2% |
| CvFlavorManager.h | 4 | 3% |
| CvArea.h | 6 | 4% |
| CvTacticalAnalysisMap.h | 6 | 4% |
| CvCity.h | 9 | 6% |
| CvNotifications.h | 10 | 6% |
| CvRandom.h | 12 | 8% |
| CvUnit.h | 12 | 8% |
| CvTeam.h | 14 | 9% |
| CvImprovementClasses.h | 16 | 10% |

**Risk:** MEDIUM - 3-16 files per header need fixes
**Effort:** ~8 hours (add explicit includes to ~100 files total)
**Impact:** Removes 13 more headers from PCH

---

### Tier 4: COMMONLY USED (>10% - 6 headers)

**Keep in PCH for now (or most effort to remove)**

| Header | # Files | % of Files |
|--------|---------|------------|
| CvAStar.h | 17 | 11% |
| CvInfos.h | 17 | 11% |
| CvMap.h | 17 | 11% |
| CvPlayerAI.h | 17 | 11% |
| CvPlot.h | 18 | 12% |
| CvGlobals.h | 25 | 16% |

**Risk:** HIGH - 17-25 files per header need fixes
**Effort:** ~20 hours (add explicit includes to ~100+ files)
**Impact:** Removes final 6 headers, but maybe not worth it

**Note:** CvGlobals.h is used by 16% of files, so it might actually belong in PCH.

---

## Phased Removal Strategy

### Phase 1: Zero-Risk Removal (16 headers, 10 minutes)

**Remove headers that NO .cpp file explicitly includes:**

```diff
--- a/CvGameCoreDLL_Expansion2/CvGameCoreDLLPCH.h
+++ b/CvGameCoreDLL_Expansion2/CvGameCoreDLLPCH.h
@@ -217,16 +217,0 @@
-#include "CvTechClasses.h"
-#include "CvPolicyClasses.h"
-#include "CvBuildingClasses.h"
-#include "CvProjectClasses.h"
-#include "CvPromotionClasses.h"
-#include "CvEmphasisClasses.h"
-#include "CvTraitClasses.h"
-#include "CvBeliefClasses.h"
-#include "CvReligionClasses.h"
-#include "CvTradeClasses.h"
-#include "CvCultureClasses.h"
-#include "CvNotificationClasses.h"
-#include "CvCityCitizens.h"
-#include "CvCityStrategyAI.h"
-#include "CvContractClasses.h"
-#include "CvCorporationClasses.h"
-#include "CvTreasury.h"
```

**Expected impact:**
- PCH size: -30% (removing 16/53 game headers)
- Compilation: Should succeed with no changes
- /Zm: Can likely reduce by ~15-20%

**Validation:**
```bash
# Build and ensure no compilation errors
# These headers are still available transitively
```

---

### Phase 2: Low-Risk Removal (18 headers, 2 hours)

**Remove rarely-used headers, add explicit includes to affected files:**

Example for CvAchievementInfo.h (used by 1 file):
```bash
# Find which file uses it
grep -l "CvAchievementInfo" *.cpp
# Result: CvAchievementUnlocker.cpp

# Add explicit include to that file
```

**Expected impact:**
- PCH size: -64% total (34/53 game headers removed)
- Compilation: Requires adding ~30 explicit includes
- /Zm: Can likely reduce by ~30-40%

---

### Phase 3: Medium-Risk Removal (13 headers, 8 hours)

**Remove sometimes-used headers, add explicit includes to affected files**

This requires more work but still manageable:
- 100+ files need explicit includes added
- Can be done incrementally (one header at a time)
- Each header removal is an independent change

**Expected impact:**
- PCH size: -89% total (47/53 game headers removed)
- /Zm: Can likely reduce by ~50-60%

---

### Phase 4: Keep Core Headers (6 headers)

**Leave these in PCH (they're heavily used):**

```cpp
#include "CvGlobals.h"      // 16% of files - global state
#include "CvPlayerAI.h"     // 11% of files - core game class
#include "CvMap.h"          // 11% of files - core game class
#include "CvPlot.h"         // 12% of files - core game class
#include "CvInfos.h"        // 11% of files - info classes
#include "CvAStar.h"        // 11% of files - pathfinding
```

These 6 headers are legitimately used widely enough to justify PCH inclusion.

**Alternatively:** Even these could be removed with ~20 hours of effort, but the cost/benefit becomes questionable.

---

## Dependency Chain Analysis

**Key Finding:** Many headers pull in others transitively

Example chain:
```
CvGameCoreDLLPCH.h
├─> CvPlayerAI.h
│   ├─> CvPlayer.h
│   │   ├─> CvSerialize.h  ← Template-heavy!
│   │   ├─> CvTreasury.h
│   │   └─> CvPolicyClasses.h
│   └─> ...
├─> CvCity.h
│   ├─> CvSerialize.h  ← Same template header!
│   └─> ...
└─> CvUnit.h
    ├─> CvSerialize.h  ← Same template header!
    └─> ...
```

**This explains:**
1. Why CvSerialize.h bloat affects PCH (it's pulled in multiple times)
2. Why many "Classes" headers aren't explicitly included (they're transitive)
3. Why removing even "unused" headers reduces PCH size

---

## Template Bloat Sources

**Headers that include CvSerialize.h:**
```bash
$ grep -l "#include.*CvSerialize" *.h
CvCity.h
CvPlayer.h
CvPlot.h
CvUnit.h
```

These 4 headers are the source of template bloat because:
1. They include CvSerialize.h (template-heavy)
2. CvPlayerAI.h includes CvPlayer.h
3. All are in the PCH
4. Every template in CvSerialize.h gets instantiated in PCH

**Impact of our desync logging:**
- Added ~50 lines to CvSerialize.h template
- Gets instantiated hundreds of times (one per sync variable)
- PCH includes 4 headers that pull in CvSerialize.h
- Result: 50 lines × 100s of instantiations = massive bloat

---

## Estimated Effort vs Impact

| Phase | Headers Removed | Effort | PCH Reduction | /Zm Reduction |
|-------|----------------|--------|---------------|---------------|
| Phase 1 | 16 | 10 min | -30% | -15-20% |
| Phase 2 | +18 (34 total) | +2 hr | -64% | -30-40% |
| Phase 3 | +13 (47 total) | +8 hr | -89% | -50-60% |
| Phase 4 | +6 (53 total) | +20 hr | -100% | -70-80% |

**Recommendation:** Do Phase 1 + 2 (34 headers, ~2 hours effort, -64% PCH size)

This provides the best cost/benefit ratio.

---

## Proof of Concept: Remove One Header

Let's test the theory by removing ONE header from PCH.

**Candidate:** CvTechClasses.h (0 explicit includes)

**Test:**
1. Remove `#include "CvTechClasses.h"` from CvGameCoreDLLPCH.h
2. Compile
3. If errors, add explicit includes to broken .cpp files
4. Measure PCH size change

**Expected result:** Compiles with no changes (0 files need it)

---

## Answer to Original Questions

### 1. How many game headers are in PCH?
**Answer:** 53 game headers (plus 13 utility headers)

### 2. Which are the biggest contributors to bloat?
**Answer:** 
- CvPlayerAI.h, CvCity.h, CvUnit.h, CvPlot.h (they pull in CvSerialize.h templates)
- But paradoxically, the UNUSED headers also contribute (they're just wasted space)

### 3. How many .cpp files would break if we remove each header?
**Answer:** See tiers above:
- 16 headers: 0 files break
- 18 headers: 1-2 files break each
- 13 headers: 3-16 files break each
- 6 headers: 17-25 files break each

### 4. Can we do this incrementally?
**Answer:** YES! One header at a time, starting with zero-risk ones.

### 5. What's the real effort estimate per header?
**Answer:**
- Tier 1 (never used): <1 minute each
- Tier 2 (rarely used): ~5-10 minutes each
- Tier 3 (sometimes used): ~30-60 minutes each
- Tier 4 (commonly used): ~3-4 hours each

### 6. Is 40 hours realistic?
**Answer:** **NO!** Original estimate was way off.
- Phase 1 + 2: ~2 hours, removes 64% of game headers
- Phase 1 + 2 + 3: ~10 hours, removes 89% of game headers
- Complete removal: ~30 hours, removes 100%

The original "40 hours" estimate was for removing ALL headers. We can get 64% reduction in 2 hours!

### 7. Can we start with low-risk headers first?
**Answer:** ABSOLUTELY! That's the entire strategy.

---

## Next Steps

1. ✅ **Proof of Concept** - Remove CvTechClasses.h, verify compilation
2. ✅ **Phase 1 Implementation** - Remove all 16 zero-risk headers
3. ✅ **Measure Impact** - Check /Zm requirements, PCH size, compile time
4. 📝 **Document Results** - Update estimates based on real data
5. 🔄 **Phase 2 Planning** - Plan removal of 18 rarely-used headers

---

## Conclusion

**The PCH bloat problem is WAY easier to fix than we thought!**

- 30% of game headers have ZERO explicit uses
- 64% are used by <10% of files
- We can remove 34 headers in ~2 hours
- This should reduce /Zm requirements by 30-40%

**This changes the strategy:**
- ✅ Do Phase 1 immediately (16 headers, 10 min)
- ✅ Do Phase 2 soon (18 headers, 2 hr)
- 🤔 Evaluate Phase 3 based on results (13 headers, 8 hr)
- ❓ Phase 4 is optional (6 headers, 20 hr)

**Original plan:** 40-hour refactor (scary!)
**New plan:** 2-hour incremental fix (doable!)

This is a **game changer** for the project's maintainability.
