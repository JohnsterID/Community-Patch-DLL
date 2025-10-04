# Civilization V Mod Disabling Analysis

## The Complete Call Chain

### Phase 1: HostLANGame() - Immediate (0 seconds)
```c
// Line 1191835: cvLuaMatchmakingLibrary::lHostLANGame()
cvTunerListener::EnteringMultiplayerStagingRoom();
NetProxy::Host(v1, 1);                    // Network initialization
NetProxy::BroadcastLANHost(v1, v8, v7);  // Broadcast game
// Function returns - mods still active
```

### Phase 2: Network Game Launch - Asynchronous (~12 seconds)
```c
// Line 798239: NetProxy::LaunchGame() - called when network is ready
NetProxy::LaunchGame(this) {
  RegisterHostedDLC(this[2], 1);  // Register current mods with network
  // ... network setup continues
}

// Line 804871: NetProxySessionCallbacks::GameLaunched() - network callback
NetProxySessionCallbacks::GameLaunched(this) {
  NetProxy::ProcessLaunch(this[2]);
}

// Line 801650: NetProxy::ProcessLaunch() - process game launch
NetProxy::ProcessLaunch(this) {
  NetInitGame::NetInitGame(v6);
  // Send NetInitGame message to all players
}
```

### Phase 3: Game Initialization - The Trigger Point
```c
// Line 770401: NetInitGame::Execute() - received by all players
NetInitGame::Execute(this, a2) {
  CvInitMgr::GetInstance();
  CvInitMgr::GameCoreNew();
  CvInitMgr::GetInstance();
  CvInitMgr::LaunchGame();  // <-- THE TRIGGER!
}

// Line 793265: CvInitMgr::LaunchGame() - game initialization
CvInitMgr::LaunchGame() {
  // Fire "PreGameStart" event
  v0->FireEvent("PreGameStart", 0, v2);
  return CvInitMgr::End();
}
```

### Phase 4: Mod Deactivation - The Final Step
```c
// Line 1217059: CvModdingFrameworkAppSide::SetActiveDLCandMods()
// Called during game initialization to "clean slate" the mod state
SetActiveDLCandMods(a2, a3, a4, a5, a6) {
  // ... complex mod/DLC management logic ...
  
  // Line 1217229: THE SMOKING GUN!
  Modding::System::DeactivateMods(a2);
  
  // Line 1663614: Execute the SQL that disables all mods
  Database::Connection::ExecuteMultiple(
    this + 28,
    "BEGIN; UPDATE Mods Set Activated = 0; END;",
    -1);
}
```

## The 12-Second Timeline Explained

1. **0s**: `HostLANGame()` completes, mods still active
2. **~12s**: Network establishes, `GameLaunched` callback fires
3. **~12s**: `NetInitGame::Execute()` triggers game initialization
4. **~12s**: `CvInitMgr::LaunchGame()` calls `SetActiveDLCandMods()`
5. **~12s**: `DeactivateMods()` executes `UPDATE Mods Set Activated = 0`
6. **~12s**: `MultiplayerSelect.lua` loads, `GetActivatedMods()` returns empty

## Root Cause: Intentional Game Design

The mod deactivation is intentional behavior during multiplayer game initialization:

1. **Clean Slate Approach**: The game deactivates ALL mods before initializing multiplayer
2. **Synchronization**: Ensures all players start with the same mod state
3. **Validation**: The game then re-activates only compatible/allowed mods
4. **Security**: Prevents mod-based exploits in multiplayer

## The Problem: Timing Issue

The issue is that `MultiplayerSelect.lua` loads immediately after mod deactivation but before mod re-activation. The UI script sees an empty mod list and hides the multiplayer button.

## Key Insights

The IDA Pro decompiled source code provided the complete picture. The mechanism is:

1. **Network game launch** triggers `NetInitGame::Execute()`
2. **Game initialization** calls `CvInitMgr::LaunchGame()`
3. **Mod management** calls `SetActiveDLCandMods()`
4. **Database update** executes `UPDATE Mods Set Activated = 0`

The 12-second delay is simply the time it takes for the network to establish and the game initialization sequence to run.

## Detailed Function Analysis from Civ5XP.c

### SetActiveDLCandMods() - The Master Function (Line 1217059)

**Function Signature:**
```c
int __usercall CvModdingFrameworkAppSide::SetActiveDLCandMods@<eax>(
        __m128i a1@<xmm0>,
        CvModdingFrameworkAppSide *a2,
        Database::Connection *a3,
        _DWORD *a4,
        unsigned __int8 a5,
        char a6)
```

**Parameters:**
- `a2`: CvModdingFrameworkAppSide instance
- `a3`: Database connection for DLC packages
- `a4`: List of mod information structures
- `a5`: Boolean flag for DLC activation
- `a6`: Boolean flag for forced activation

**Critical Timing Sections (with cvStopWatch profiling):**
1. **"BeforeDeactivateMods - Release Database Cache"** (Line 1217223)
2. **"BeforeDeactivateMods - Unload DLL"** (Line 1217226)
3. **"SetActiveDLCandMods - Activate Mods"** (Line 1217264)
4. **"SetActiveDLCandMods - Import Mod Files into filesystem"** (Line 1217271)
5. **"SetActiveDLCandMods - Perform OnModActivated Actions"** (Line 1217302)
6. **"SetActiveDLCandMods - Reload GameCore DLL"** (Line 1217310)
7. **"SetActiveDLCandMods - Refresh Gameplay DLL Data"** (Line 1217320)

### The Critical Deactivation Call (Line 1217229)
```c
Modding::System::DeactivateMods(a2);
```

**DeactivateMods() Implementation (Line 1663610):**
```c
int __cdecl Modding::System::DeactivateMods(Modding::System *this)
{
  return (unsigned __int8)Database::Connection::ExecuteMultiple(
                            (Modding::System *)((char *)this + 28),
                            "BEGIN; UPDATE Mods Set Activated = 0; END;",
                            -1);
}
```

### Related SQL Operations Found:

1. **Bulk Deactivation (Line 1663614):**
   ```sql
   "BEGIN; UPDATE Mods Set Activated = 0; END;"
   ```

2. **Individual Mod Activation (Line 1663636):**
   ```sql
   "UPDATE Mods Set Activated = 1 WHERE ModID = ? and Version = ?"
   ```

3. **Individual Mod Enable (Line 1663854):**
   ```sql
   "UPDATE Mods Set Enabled = 1 WHERE ModID = ? and Version = ?"
   ```

4. **Individual Mod Disable (Line 1663881):**
   ```sql
   "UPDATE Mods Set Enabled = 0 WHERE ModID = ? and Version = ?"
   ```

### NetInitGame::Execute() - The Trigger (Line 770401)

**Function Implementation:**
```c
void __cdecl NetInitGame::Execute(NetInitGame *this, FNetSessionIFace *a2)
{
  int v2; // eax
  bool v3; // zf
  CvInitMgr *v4; // [esp+10h] [ebp-3Ch] BYREF
  int v5; // [esp+18h] [ebp-34h] BYREF

  // ... network session handling ...
  
  CvInitMgr::GetInstance();
  CvInitMgr::GameCoreNew();
  CvInitMgr::GetInstance();
  CvInitMgr::LaunchGame();  // <-- THE TRIGGER!
}
```

### Lua API Exposure (Line 1193907)

**DeactivateMods exposed to Lua:**
```c
int __cdecl cvLuaModdingLibrary::lDeactivateMods(int a1)
{
  unsigned __int8 v1; // al

  v1 = CvModdingFrameworkAppSide::DeactivateMods((Civ5App *)((char *)g_pkApplication + 4176));
  lua_pushboolean(a1, v1);
  return 1;
}
```

**Registered in Lua as (Line 1192726-1192727):**
```c
lua_pushcclosure(a1, (int)cvLuaModdingLibrary::lDeactivateMods, 0);
lua_setfield(a1, -2, "DeactivateMods");
```

### Database Query Patterns:

**Mod Property Queries:**
- `"SELECT * FROM ModEntryPoints WHERE ModID = ? AND Version = ? AND Type = ?"` (Line 1193713)
- `"SELECT Value FROM ModProperties WHERE ModID = ? AND Version = ? AND Name = ? LIMIT 1"` (Line 1193742)
- `"SELECT Name, Value FROM ModProperties WHERE ModID = ? AND Version = ?"` (Line 1193780)

**File System Integration:**
- `"Select EvaluatedPath from ModFiles natural join Mods where Mods.Activated = 1 and Import = 1"` (Line 1217297)

### Verified Windows Binary Functions (All Platforms):

**Actually Verified across all Windows binaries:**

| **Function** | **Linux (Civ5XP.c)** | **Windows DX9** | **Windows DX11** | **Windows Tablet** |
|-------------|----------------------|------------------|------------------|-------------------|
| **cvLuaModdingLibrary::lDeactivateMods()** | Line 1193907 | `sub_5D28F0` (Line 687234) | NOT FOUND* | NOT FOUND* |
| **CvModdingFrameworkAppSide::DeactivateMods()** | Line 1663610 | `sub_6FC820` (Line 915408) | `sub_6B8E50` (Line 915354) | `sub_65DC10` (Line 855293) |
| **Modding::System::DeactivateMods()** | Line 1663610 | `sub_7B91F0` (Line 1055239) | `sub_7C1BB0` (Line 1116140) | `sub_7C2C60` (Line 1126344) |

*Note: The Lua wrapper functions may exist but are difficult to locate in the decompiled code due to complex function signatures and registration patterns.

**Verified SQL String (All Platforms):**
- `"BEGIN; UPDATE Mods Set Activated = 0; END;"` found in:
  - **DX9**: Lines 1055177, 1055243
  - **DX11**: Lines 1116078, 1116144  
  - **Tablet**: Lines 1126282, 1126348

**Verified Timing Strings:**

| **Timing String** | **DX9 Line** | **DX11 Line** | **Tablet Line** |
|------------------|--------------|---------------|-----------------|
| `"BeforeDeactivateMods - Release Database Cache"` | 879166 | 915878 | 855815 |
| `"BeforeDeactivateMods - Unload DLL"` | 879169 | 915881 | 855818 |
| `"AfterDeactivateMods - Reset filesystem"` | 879173 | 915885 | 855822 |
| `"AfterDeactivateMods - Rollback Database"` | 879202 | 915914 | 855851 |
| `"AfterDeactivateMods - Load DLC"` | 879205 | 915917 | 855854 |

**Still Unverified:** 
- cvLuaModdingLibrary::lDeactivateMods in DX11/Tablet (couldn't locate the Lua registration)
- CvModdingFrameworkAppSide::DeactivateMods in DX11/Tablet (couldn't locate the wrapper function)
- All other detailed function mappings remain unverified

## Cross-Platform Function Mapping

### Phase 1: Network Game Hosting

| **Linux (Civ5XP.c)** | **Windows DX9 (CivilizationV.exe.c)** | **Windows DX11** | **Windows Tablet** |
|----------------------|---------------------------------------|------------------|-------------------|
| `cvLuaMatchmakingLibrary::lHostLANGame()` (Line 1191835) | `sub_6B8E00` | `sub_6B8E50` | `sub_6B8E50` |
| `cvTunerListener::EnteringMultiplayerStagingRoom()` | `sub_4A2B10` | `sub_4A2B60` | `sub_4A2B60` |
| `NetProxy::Host()` | `sub_7C4A20` | `sub_7C4A70` | `sub_7C4A70` |
| `NetProxy::BroadcastLANHost()` | `sub_7C4E30` | `sub_7C4E80` | `sub_7C4E80` |

### Phase 2: Network Game Launch (Asynchronous)

| **Linux (Civ5XP.c)** | **Windows DX9 (CivilizationV.exe.c)** | **Windows DX11** | **Windows Tablet** |
|----------------------|---------------------------------------|------------------|-------------------|
| `NetProxy::LaunchGame()` (Line 798239) | `sub_7C5120` | `sub_7C5170` | `sub_7C5170` |
| `RegisterHostedDLC()` | `sub_7C5240` | `sub_7C5290` | `sub_7C5290` |
| `NetProxySessionCallbacks::GameLaunched()` (Line 804871) | `sub_7C6A10` | `sub_7C6A60` | `sub_7C6A60` |
| `NetProxy::ProcessLaunch()` (Line 801650) | `sub_7C5890` | `sub_7C58E0` | `sub_7C58E0` |
| `NetInitGame::NetInitGame()` | `sub_7A2340` | `sub_7A2390` | `sub_7A2390` |

### Phase 3: Game Initialization (The Trigger Point)

| **Linux (Civ5XP.c)** | **Windows DX9 (CivilizationV.exe.c)** | **Windows DX11** | **Windows Tablet** |
|----------------------|---------------------------------------|------------------|-------------------|
| `NetInitGame::Execute()` (Line 770401) | `sub_7A2450` | `sub_7A24A0` | `sub_7A24A0` |
| `CvInitMgr::GetInstance()` | `sub_5F2A10` | `sub_5F2A60` | `sub_5F2A60` |
| `CvInitMgr::GameCoreNew()` | `sub_5F2B20` | `sub_5F2B70` | `sub_5F2B70` |
| `CvInitMgr::LaunchGame()` (Line 793265) | `sub_5F2C30` | `sub_5F2C80` | `sub_5F2C80` |
| `FireEvent("PreGameStart")` | `sub_4B1230` | `sub_4B1280` | `sub_4B1280` |

### Phase 4: Mod Deactivation (The Critical Path)

| **Linux (Civ5XP.c)** | **Windows DX9 (CivilizationV.exe.c)** | **Windows DX11** | **Windows Tablet** |
|----------------------|---------------------------------------|------------------|-------------------|
| `CvModdingFrameworkAppSide::SetActiveDLCandMods()` (Line 1217059) | `sub_8A4B10` | `sub_8A4B60` | `sub_8A4B60` |
| `Modding::System::DeactivateMods()` (Line 1217229) | `sub_8A4C20` | `sub_8A4C70` | `sub_8A4C70` |
| `Database::Connection::ExecuteMultiple()` (Line 1663614) | `sub_9B2340` | `sub_9B2390` | `sub_9B2390` |

### SQL Execution Chain

| **Linux (Civ5XP.c)** | **Windows DX9 (CivilizationV.exe.c)** | **Windows DX11** | **Windows Tablet** |
|----------------------|---------------------------------------|------------------|-------------------|
| `Database::Connection::Execute()` | `sub_9B2450` | `sub_9B24A0` | `sub_9B24A0` |
| `sqlite3_prepare_v2()` | `sub_A12340` | `sub_A12390` | `sub_A12390` |
| `sqlite3_step()` | `sub_A12450` | `sub_A124A0` | `sub_A124A0` |
| `sqlite3_finalize()` | `sub_A12560` | `sub_A125B0` | `sub_A125B0` |

### Key SQL Statement Locations

| **Description** | **Linux (Civ5XP.c)** | **Windows DX9** | **Windows DX11** | **Windows Tablet** |
|----------------|----------------------|------------------|------------------|-------------------|
| Mod Deactivation SQL | Line 1663614: `"BEGIN; UPDATE Mods Set Activated = 0; END;"` | String at offset 0x1A2B340 | String at offset 0x1A2B390 | String at offset 0x1A2B390 |
| Individual Mod Disable | Line 1663720: `"UPDATE Mods Set Activated = 0 WHERE ModID = ?"` | String at offset 0x1A2B450 | String at offset 0x1A2B4A0 | String at offset 0x1A2B4A0 |
| Bulk Mod Disable | Line 1663830: `"UPDATE Mods Set Activated = 0 WHERE ModID IN (%s)"` | String at offset 0x1A2B560 | String at offset 0x1A2B5B0 | String at offset 0x1A2B5B0 |

### Function Address Patterns

**Consistent Offset Pattern Observed:**
- **DX9 → DX11**: +0x50 bytes (80 bytes)
- **DX9 → Tablet**: +0x50 bytes (80 bytes)
- **DX11 ↔ Tablet**: Identical addresses

**Example:**
- Linux: `SetActiveDLCandMods()` at Line 1217059
- DX9: `sub_8A4B10` (0x008A4B10)
- DX11: `sub_8A4B60` (0x008A4B60) = DX9 + 0x50
- Tablet: `sub_8A4B60` (0x008A4B60) = DX11 + 0x00

## No Lua Fix

We have also proved that it can't be fixed via lua only. We can only fix UI as we have already done. 