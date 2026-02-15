# Trade Route Memory Crash Research

## Issue Summary
Late-game crash in `TradeConnection::TradeConnection` (copy constructor, line 29) due to memory exhaustion during trade route evaluation.

## Crash Details
```
Exception: 0xC0000005: Access violation writing location 0x0003B598
Location: CvGameCore_Expansion2.dll!TradeConnection::TradeConnection Line 29
_Newvec: 0x00000000 <NULL>  ← Vector reallocation failed
Vector size: 1066 elements
```

### Call Stack
```
TradeConnection::TradeConnection (copy ctor) Line 29
std::vector<TradeConnection>::push_back
CvTradeAI::GetAvailableTR Line 5896
CvTradeAI::GetPrioritizedTradeRoutes
CvHomelandAI::ExecuteTradeUnitMoves
```

## Root Cause Analysis

### Memory Exhaustion Timeline
VS Output shows **27 `std::bad_alloc` exceptions** starting at Turn 239, escalating through Turn 240 before final crash. Memory was already exhausted BEFORE GetAvailableTR ran.

### The Real Memory Hog: TradePathLookup Cache

The trade path cache stores full `SPath` objects:
```cpp
typedef std::map<int,std::map<int,SPath>> TradePathLookup;

struct SPath {
    std::vector<SPathNode> vPlots;  // <-- THIS IS THE PROBLEM
    int iTotalCost;
    int iNormalizedDistanceRaw;
    int iTotalTurns;
    int iTurnSliceGenerated;
    SPathFinderUserData sConfig;
};

struct SPathNode {
    short x, y, turns, moves;  // 8 bytes per node
};
```

### Memory Calculation
- Long trade routes: 5000-6200 nodes
- Each SPathNode: 8 bytes
- Per path: 40-50KB
- Thousands of cached paths across all players
- **Total: Potentially 100MB+ just in path cache**

## Previous Fix Attempt (482e8cd74)

### What It Did
1. Removed `CopyPathIntoTradeConnection()` call in GetAvailableTR
2. Added `reserve(2000)` for the TradeConnection vector
3. Added `GetCachedTradePath()` helper for scoring functions

### Why It's Insufficient
- The fix reduced memory usage during scoring
- BUT the cache itself (`TradePathLookup`) still stores all full paths
- Memory exhaustion happens during cache BUILDING, not during scoring
- By the time GetAvailableTR runs, memory is already gone

## Key Finding: SPathNode Fields Usage

**Trade cache operations only use `x,y` fields:**
```cpp
CvPlot* SPath::get(int i) const {
    return GC.getMap().plotUnchecked(vPlots[i].x, vPlots[i].y);
}
```

**`turns` and `moves` fields are used by:**
- Unit movement (CvUnit.cpp)
- UI display (CvDllContext.cpp)
- NOT trade route evaluation/scoring

## Proposed Fix: Lightweight Cache Storage

### Option 1: Separate Trade Path Cache Structure
```cpp
struct SPathNodeLite {
    short x, y;  // 4 bytes instead of 8
};

struct SPathLite {
    std::vector<SPathNodeLite> vPlots;
    int iTotalCost;
    int iNormalizedDistanceRaw;
    int iTotalTurns;
};

// Trade cache uses SPathLite (50% memory reduction)
typedef std::map<int,std::map<int,SPathLite>> TradePathLookup;
```

### Option 2: Store Plot Indices Only
```cpp
struct SPathLite {
    std::vector<int> vPlotIndices;  // 4 bytes per node
    int iTotalCost;
    int iNormalizedDistanceRaw;
    int iTotalTurns;
};
```

### Memory Savings
- 6000 node path: 48KB → 24KB (50% reduction)
- Total cache: ~50% smaller

### Game Logic Impact: NONE
- Same paths found (same pathfinding algorithm)
- Same plots iterated (same scoring)
- Same routes selected
- Full path with turns/moves recalculated only when CREATING selected route

## Fixes That WOULD Change Game Logic (NOT Recommended)

| Fix | Impact |
|-----|--------|
| Memory pressure check before cache | AI skips evaluation = different routes |
| Limit cache to 500 routes | Optimal route might be #501 |
| SafeToEvaluateTradeRoutes() | Skipping = different AI behavior |

## Implementation Plan

1. Create `SPathLite` structure for trade cache
2. Modify `TradePathLookup` typedef to use `SPathLite`
3. Update `UpdateTradePathCache()` to store lightweight paths
4. Update `GetCachedTradePath()` to return lightweight paths
5. Modify scoring functions to use new structure
6. When creating route (`CreateTradeRoute`), recalculate full path

## Files to Modify

- `CvAStarNode.h` - Add SPathLite structure
- `CvTradeClasses.h` - Update TradePathLookup typedef
- `CvTradeClasses.cpp` - Update cache operations and scoring

## Test Cases

1. Load late-game save with 12+ civs
2. Verify no crash during AI turn
3. Verify same trade routes selected (compare logs)
4. Verify trade unit movement works correctly

## Related Files
- `/workspace/project/notes.txt` - Crash details
- `/workspace/project/vs-output.txt` - VS debugger output with bad_alloc timeline
- `/workspace/project/482e8cd74.txt` - Previous analysis document
- `/workspace/project/Logs/` - Game logs from crash session
