# PCH Removal Plan - Incremental Implementation

## Overview

Based on PCH_DEPENDENCY_ANALYSIS.md findings, we can remove **34 game headers** from the PCH with only **~2 hours** of effort, achieving a **64% reduction** in game header bloat.

---

## Phase 1: Zero-Risk Removal (16 headers, 10 minutes)

### Target Headers (0 explicit includes)

Remove these from `CvGameCoreDLLPCH.h`:

```cpp
// REMOVE THESE (lines 218-233, approximate):
#include "CvTreasury.h"                // 0 explicit includes
#include "CvTechClasses.h"             // 0 explicit includes
#include "CvPolicyClasses.h"           // 0 explicit includes
#include "CvBuildingClasses.h"         // 0 explicit includes
#include "CvProjectClasses.h"          // 0 explicit includes
#include "CvPromotionClasses.h"        // 0 explicit includes
#include "CvEmphasisClasses.h"         // 0 explicit includes
#include "CvTraitClasses.h"            // 0 explicit includes
#include "CvBeliefClasses.h"           // 0 explicit includes
#include "CvReligionClasses.h"         // 0 explicit includes
#include "CvTradeClasses.h"            // 0 explicit includes
#include "CvCultureClasses.h"          // 0 explicit includes
#include "CvNotificationClasses.h"     // 0 explicit includes
#include "CvCityCitizens.h"            // 0 explicit includes
#include "CvCityStrategyAI.h"          // 0 explicit includes
#include "CvContractClasses.h"         // 0 explicit includes
#include "CvCorporationClasses.h"      // 0 explicit includes
```

### Implementation Steps

1. **Edit CvGameCoreDLLPCH.h:**
   - Remove the 17 lines listed above
   - Add comment explaining why (with reference to this doc)

2. **Clean and rebuild:**
   ```bash
   # MSVC
   msbuild VoxPopuli.sln /t:Clean
   msbuild VoxPopuli.sln /t:Build /p:Configuration=Release
   
   # Clang
   python build_vp_clang.py --clean
   python build_vp_clang.py
   ```

3. **Verify compilation succeeds:**
   - Should compile with **ZERO errors**
   - These headers are still available transitively through other includes

4. **Measure impact:**
   - Check if /Zm can be reduced
   - Note PCH compile time change
   - Document in commit message

### Expected Results

- ✅ Compilation: Success (no code changes needed)
- ✅ PCH size: -30% game headers (16/53 removed)
- ✅ /Zm: Can likely reduce by 15-20%
- ✅ Risk: None (can be reverted if issues found)

### Example Commit

```
Remove 16 unused game headers from PCH

These headers are not explicitly included by any .cpp file in the project.
They were added to the PCH but are only used transitively through other
includes, making them redundant.

Headers removed:
- CvTechClasses.h, CvPolicyClasses.h, CvBuildingClasses.h
- CvProjectClasses.h, CvPromotionClasses.h, CvEmphasisClasses.h
- CvTraitClasses.h, CvBeliefClasses.h, CvReligionClasses.h
- CvTradeClasses.h, CvCultureClasses.h, CvNotificationClasses.h
- CvTreasury.h, CvCityCitizens.h, CvCityStrategyAI.h
- CvContractClasses.h, CvCorporationClasses.h

Impact:
- PCH size: -30% (game headers: 53 → 37)
- No compilation errors (verified on MSVC and Clang)
- Reduces PCH memory pressure

See PCH_DEPENDENCY_ANALYSIS.md for detailed analysis.

Files changed:
- CvGameCoreDLL_Expansion2/CvGameCoreDLLPCH.h (-17 lines)
```

---

## Phase 2: Low-Risk Removal (18 headers, ~2 hours)

### Target Headers (1-2 explicit includes each)

These require adding explicit includes to 1-2 files each.

| Header | Files to Fix | Explicit Include Needed |
|--------|--------------|------------------------|
| CvAchievementInfo.h | CvAchievementUnlocker.cpp | Yes |
| CvBuildingProductionAI.h | CvCityAI.cpp | Yes |
| CvCityAI.h | CvDiplomacyAI.cpp | Yes |
| CvGreatPersonInfo.h | CvInfos.cpp | Yes |
| CvPlotInfo.h | CvInfos.cpp | Yes |
| CvProcessProductionAI.h | CvCityAI.cpp | Yes |
| CvProjectProductionAI.h | CvCityAI.cpp | Yes |
| CvTraitClasses.h | CvInfos.cpp | Yes |
| CvUnitProductionAI.h | CvCityAI.cpp | Yes |
| CvAdvisorCounsel.h | 2 files | Yes |
| CvAdvisorRecommender.h | 2 files | Yes |
| CvDealClasses.h | 2 files | Yes |
| CvEspionageClasses.h | 2 files | Yes |
| CvEventLog.h | 2 files | Yes |
| CvHomelandAI.h | 2 files | Yes |
| CvSiteEvaluationClasses.h | 2 files | Yes |
| CvUnitClasses.h | 2 files | Yes |
| CvVotingClasses.h | 2 files | Yes |

### Implementation Steps (Per Header)

**Example: CvAchievementInfo.h**

1. **Find files that need it:**
   ```bash
   cd CvGameCoreDLL_Expansion2
   grep -l "CvAchievement" *.cpp
   # Result: CvAchievementUnlocker.cpp
   ```

2. **Remove from PCH:**
   ```diff
   --- a/CvGameCoreDLL_Expansion2/CvGameCoreDLLPCH.h
   +++ b/CvGameCoreDLL_Expansion2/CvGameCoreDLLPCH.h
   @@ -247,1 +247,0 @@
   -#include "CvAchievementInfo.h"
   ```

3. **Add explicit include:**
   ```diff
   --- a/CvGameCoreDLL_Expansion2/CvAchievementUnlocker.cpp
   +++ b/CvGameCoreDLL_Expansion2/CvAchievementUnlocker.cpp
   @@ -8,0 +8,1 @@
   +#include "CvAchievementInfo.h"
   ```

4. **Compile and verify:**
   ```bash
   msbuild VoxPopuli.sln /t:Build /p:Configuration=Release
   ```

5. **Commit:**
   ```
   Remove CvAchievementInfo.h from PCH
   
   Only used by CvAchievementUnlocker.cpp, which now includes it explicitly.
   ```

### Automation Script

```bash
#!/bin/bash
# remove_header_from_pch.sh <header_name>

HEADER=$1
PCH_FILE="CvGameCoreDLL_Expansion2/CvGameCoreDLLPCH.h"

# Find files that use this header
FILES=$(grep -l "${HEADER%.h}" CvGameCoreDLL_Expansion2/*.cpp)

echo "Header: $HEADER"
echo "Files that use it:"
echo "$FILES"
echo ""

# Remove from PCH
echo "Removing from PCH..."
sed -i "/#include \"$HEADER\"/d" "$PCH_FILE"

# Add explicit includes
for file in $FILES; do
    echo "Adding explicit include to $file..."
    # Add after PCH include (usually line 2)
    sed -i "2a #include \"$HEADER\"" "$file"
done

echo "Done! Now compile to verify."
```

### Phase 2 Tracking Checklist

- [ ] CvAchievementInfo.h
- [ ] CvBuildingProductionAI.h
- [ ] CvCityAI.h
- [ ] CvGreatPersonInfo.h
- [ ] CvPlotInfo.h
- [ ] CvProcessProductionAI.h
- [ ] CvProjectProductionAI.h
- [ ] CvTraitClasses.h
- [ ] CvUnitProductionAI.h
- [ ] CvAdvisorCounsel.h
- [ ] CvAdvisorRecommender.h
- [ ] CvDealClasses.h
- [ ] CvEspionageClasses.h
- [ ] CvEventLog.h
- [ ] CvHomelandAI.h
- [ ] CvSiteEvaluationClasses.h
- [ ] CvUnitClasses.h
- [ ] CvVotingClasses.h

### Expected Results After Phase 2

- ✅ PCH size: -64% game headers (34/53 removed)
- ✅ /Zm: Can likely reduce by 30-40%
- ✅ ~30 files modified (added explicit includes)
- ✅ Risk: Low (easy to revert per-header)

---

## Phase 3: Medium-Risk Removal (13 headers, ~8 hours)

**Target headers used by 3-16 files each**

This requires more work but is still manageable. Do incrementally:

### Headers by Effort

| Header | Files | Est. Time |
|--------|-------|-----------|
| CvBuilderTaskingAI.h | 3 | 15 min |
| CvCityConnections.h | 3 | 15 min |
| CvGame.h | 3 | 15 min |
| CvTacticalAI.h | 3 | 15 min |
| CvFlavorManager.h | 4 | 20 min |
| CvArea.h | 6 | 30 min |
| CvTacticalAnalysisMap.h | 6 | 30 min |
| CvCity.h | 9 | 45 min |
| CvNotifications.h | 10 | 50 min |
| CvRandom.h | 12 | 1 hr |
| CvUnit.h | 12 | 1 hr |
| CvTeam.h | 14 | 1 hr 10 min |
| CvImprovementClasses.h | 16 | 1 hr 20 min |

**Total:** ~8 hours

### Strategy

Do one header per day/session:
1. Pick header from list
2. Find files that use it
3. Remove from PCH
4. Add explicit includes
5. Compile and test
6. Commit
7. Repeat

**Can be done over 2 weeks at 30 min/day pace.**

---

## Phase 4: Keep Core Headers (6 headers)

**These are used by 11-16% of files - probably should stay in PCH:**

```cpp
#include "CvGlobals.h"      // 25 files (16%)
#include "CvPlot.h"         // 18 files (12%)
#include "CvPlayerAI.h"     // 17 files (11%)
#include "CvMap.h"          // 17 files (11%)
#include "CvInfos.h"        // 17 files (11%)
#include "CvAStar.h"        // 17 files (11%)
```

### Recommendation: **Keep these 6**

**Why:**
- They're widely used (>10% of files)
- Removing them requires 17-25 explicit includes each
- Effort: ~20 hours for marginal benefit
- They legitimately belong in PCH

**However:** Could still remove if needed (just more work).

---

## /Zm Reduction Strategy

After each phase, test reducing /Zm:

### Current Values
```xml
<AdditionalOptions>/Oy- /Zm330 %(AdditionalOptions)</AdditionalOptions>  <!-- Debug -->
<AdditionalOptions>/Zm300 %(AdditionalOptions)</AdditionalOptions>       <!-- Release -->
```

### Try Reducing After Phase 1
```xml
<AdditionalOptions>/Oy- /Zm280 %(AdditionalOptions)</AdditionalOptions>  <!-- -15% -->
<AdditionalOptions>/Zm255 %(AdditionalOptions)</AdditionalOptions>       <!-- -15% -->
```

### Try Reducing After Phase 2
```xml
<AdditionalOptions>/Oy- /Zm230 %(AdditionalOptions)</AdditionalOptions>  <!-- -30% -->
<AdditionalOptions>/Zm210 %(AdditionalOptions)</AdditionalOptions>       <!-- -30% -->
```

### Try Reducing After Phase 3
```xml
<AdditionalOptions>/Oy- /Zm180 %(AdditionalOptions)</AdditionalOptions>  <!-- -45% -->
<AdditionalOptions>/Zm165 %(AdditionalOptions)</AdditionalOptions>       <!-- -45% -->
```

**Process:**
1. Reduce /Zm by 15%
2. Clean build
3. If fails with C3859/C1076, increase by 5%
4. Repeat until successful
5. Document final values

---

## Testing Strategy

### After Each Header Removal

1. **Clean build:**
   ```bash
   msbuild VoxPopuli.sln /t:Clean
   msbuild VoxPopuli.sln /t:Build /p:Configuration=Debug
   msbuild VoxPopuli.sln /t:Build /p:Configuration=Release
   ```

2. **Clang build:**
   ```bash
   python build_vp_clang.py --clean
   python build_vp_clang.py
   ```

3. **Smoke test:**
   - Load DLL in game
   - Start new game
   - Play for 10 turns
   - Verify no crashes

### After Complete Phase

1. **Full rebuild:**
   ```bash
   git clean -fdx CvGameCoreDLL_Expansion2/
   # Rebuild from scratch
   ```

2. **Regression testing:**
   - Run any existing tests
   - Play-test multiplayer (for desync issues)
   - Check AI behavior

---

## Rollback Plan

If any phase causes issues:

### Per-Header Rollback

```bash
# Revert specific header
git revert <commit_hash>

# Or manual:
# 1. Add header back to PCH
# 2. Remove explicit includes from .cpp files
# 3. Rebuild
```

### Full Phase Rollback

```bash
# Revert entire phase
git revert <phase_start_commit>..<phase_end_commit>
```

### Emergency Rollback

```bash
# Revert all PCH changes
git checkout master -- CvGameCoreDLL_Expansion2/CvGameCoreDLLPCH.h
git checkout master -- CvGameCoreDLL_Expansion2/*.cpp
```

---

## Success Metrics

### After Phase 1 (16 headers)
- ✅ Compilation succeeds
- ✅ PCH compile time: -10-15%
- ✅ /Zm: Reduce by 15-20%
- ✅ No runtime issues

### After Phase 2 (34 headers total)
- ✅ Compilation succeeds
- ✅ PCH compile time: -30-40%
- ✅ /Zm: Reduce by 30-40%
- ✅ 30 files with explicit includes
- ✅ No runtime issues

### After Phase 3 (47 headers total)
- ✅ Compilation succeeds
- ✅ PCH compile time: -50-60%
- ✅ /Zm: Reduce by 45-55%
- ✅ 130+ files with explicit includes
- ✅ No runtime issues

---

## Timeline

| Phase | Effort | Duration | Dependencies |
|-------|--------|----------|--------------|
| Phase 1 | 10 min | Same day | None |
| Phase 2 | 2 hr | 1 week | Phase 1 success |
| Phase 3 | 8 hr | 2 weeks | Phase 2 success |

**Total timeline:** 3-4 weeks at relaxed pace

**Or accelerated:** 1-2 days if dedicated effort

---

## Documentation Updates

After implementation, update:

1. **PCH_BLOAT_ANALYSIS.md**
   - Add "Results" section with actual measurements
   - Update effort estimates based on reality

2. **README or CONTRIBUTING**
   - Add guidelines: "Don't add headers to PCH unless >20% of files use them"
   - Document process for adding new headers

3. **Commit messages**
   - Reference this plan
   - Include metrics (PCH size change, /Zm change)

---

## Next Steps

1. ✅ Review this plan
2. 🚀 Execute Phase 1 (10 minutes)
3. 📊 Measure results
4. ✅ Update plan based on findings
5. 🚀 Execute Phase 2 (2 hours)
6. 🎯 Decide on Phase 3 based on results

---

## Appendix: Helper Scripts

### find_header_usage.sh
```bash
#!/bin/bash
# Find which .cpp files use a header
HEADER=$1
grep -n "#include.*$HEADER" CvGameCoreDLL_Expansion2/*.cpp
```

### measure_pch_size.sh
```bash
#!/bin/bash
# Measure PCH file size (Windows)
ls -lh CvGameCoreDLL_Expansion2/Debug/*.pch
ls -lh CvGameCoreDLL_Expansion2/Release/*.pch
```

### test_zm_reduction.sh
```bash
#!/bin/bash
# Test if lower /Zm value works
ZM_VALUE=$1

# Update VoxPopuli.vcxproj
sed -i "s|/Zm[0-9]*|/Zm$ZM_VALUE|g" CvGameCoreDLL_Expansion2/VoxPopuli.vcxproj

# Try to build
msbuild VoxPopuli.sln /t:Build /p:Configuration=Release

if [ $? -eq 0 ]; then
    echo "✅ /Zm$ZM_VALUE works!"
else
    echo "❌ /Zm$ZM_VALUE failed"
fi
```
