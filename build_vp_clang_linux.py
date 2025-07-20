import os
import subprocess
from enum import Enum
import typing
import time
import tempfile
from pathlib import Path
import argparse
from queue import Queue

class Config(Enum):
    Release = 0
    Debug = 1

CORE_DLL = 'CvGameCore_Expansion2'
PROJECT_DIR = Path().resolve()
SDK_VERSION = '7.0A'
# Resolve relative paths to absolute (Linux-style paths)
relative_include_1 = os.path.abspath('./Dependencies/v7.0a_include')
relative_include_2 = os.path.abspath('./Dependencies/vc9_include')
relative_lib_1 = os.path.abspath('./Dependencies/v7.0a_lib')
relative_lib_2 = os.path.abspath('./Dependencies/vc9_lib')
INCLUDE_PATHS = [
    relative_include_1,
    relative_include_2
]
LIB_PATHS = [
    relative_lib_1,
    relative_lib_2
]
BUILD_DIR = {
    Config.Release: 'clang-build/Release',
    Config.Debug: 'clang-build/Debug',
}
OUT_DIR = {
    Config.Release: 'clang-output/Release',
    Config.Debug: 'clang-output/Debug',
}
LIBS = [
    'CvWorldBuilderMap/lib/CvWorldBuilderMapWin32.obj',
    'CvGameCoreDLLUtil/lib/CvGameCoreDLLUtilWin32.lib',
    'CvLocalization/lib/CvLocalizationWin32.lib',
    'CvGameDatabase/lib/CvGameDatabaseWin32.lib',
    'FirePlace/lib/FireWorksWin32.obj',
    'FirePlace/lib/FLuaWin32.lib',
    'ThirdPartyLibs/Lua51/lib/lua51_Win32.lib',
]
DEFAULT_LIBS = [
    'WinMM.Lib',
    'Kernel32.Lib',
    'User32.Lib',
    'Gdi32.Lib',
    'WinSpool.Lib',
    'ComDlg32.Lib',
    'AdvAPI32.Lib',
    'shell32.lib',
    'Ole32.Lib',
    'OleAut32.Lib',
    'Uuid.Lib',
    'odbc32.lib',
    'odbccp32.lib',
    'msvcrt.lib',        # From vc9_lib (dynamic C runtime) - matches Windows build /MD
    'DbgHelp.Lib',       # From v7.0a_lib
    'comsuppw.lib',      # From vc9_lib
    'msvcprt.lib',       # From vc9_lib (dynamic C++ runtime) - matches Windows build /MD
    'OLDNAMES.LIB',      # From vc9_lib
    'Version.Lib',       # From v7.0a_lib
    'Psapi.Lib',         # From v7.0a_lib
]
DEF_FILE = 'CvGameCoreDLL_Expansion2/CvGameCoreDLL.def'
INCLUDE_DIRS = [
    'CvGameCoreDLL_Expansion2',
    'CvWorldBuilderMap/include',
    'CvGameCoreDLLUtil/include',
    'CvLocalization/include',
    'CvGameDatabase/include',
    'ThirdPartyLibs/Lua51/include'
]

# Third-party library headers (treated as system headers to suppress warnings)
SYSTEM_INCLUDE_DIRS = [
    'FirePlace/include',
    'FirePlace/include/FireWorks',
]
SHARED_PREDEFS = [
    'FXS_IS_DLL',
    'WIN32',
    '_WINDOWS',
    '_USRDLL',
    'EXTERNAL_PAUSING',
    'CVGAMECOREDLL_EXPORTS',
    'FINAL_RELEASE',
    '_CRT_SECURE_NO_WARNINGS',
    '_WINDLL',
    # Disable problematic Windows SAL annotations for clang compatibility
    '_USE_DECLSPECS_FOR_SAL=0',
    '_USE_ATTRIBUTES_FOR_SAL=0',
    '__SAL_H_VERSION=0',
    # Individual driver macros - define properly to avoid token pasting errors
    '__drv_functionClass(x)=',
    '__drv_out(x)=',
    '__drv_in(x)=', 
    '__drv_inout(x)=',
    '__drv_declspec(x)=',
    '__drv_nop(x)=x',  # Must expand to x for token pasting to work
    '__drv_when(x,y)=',
    '__drv_at(x,y)=',
    '__drv_group(x)=',
    '__$drv_group(x)=',
    '__pre=',
    '__post=',
]
RELEASE_PREDEFS = SHARED_PREDEFS + ['STRONG_ASSUMPTIONS', 'NDEBUG', 'VPRELEASE_ERRORMSG']
DEBUG_PREDEFS = SHARED_PREDEFS + ['VPDEBUG']
PREDEFS = {
    Config.Release: RELEASE_PREDEFS,
    Config.Debug: DEBUG_PREDEFS,
}
CL_SUPPRESS = [
    # Core compatibility warnings for cross-compilation
    'invalid-offsetof',               # offsetof() usage in project code
    'tautological-constant-out-of-range-compare',  # Range comparisons in project code
    'comment',                        # Comment formatting issues
    # 'enum-constexpr-conversion',    # TODO: #9786 - enum conversion issues (not supported in clang 14)
    'c++11-narrowing',               # Narrowing conversions in project code
    'reserved-user-defined-literal',  # C++11 feature, we're using C++03/TR1
    'ignored-pragma-intrinsic',       # MSVC intrinsics not available in clang cross-compilation
    'pragma-pack',                    # Expected when using Windows headers with packing changes
    'nonportable-include-path',       # Expected when cross-compiling Windows headers on Linux
    
    # C++03/TR1 compatibility warnings (legacy codebase)
    'ignored-qualifiers',             # 'const' return type qualifiers (C++03 style, 8700+ warnings)
    'deprecated-copy-with-user-provided-copy',  # Implicit copy constructors/assignment (C++11+ deprecation)
]

# Additional suppressions for -Wall (MSVC /W2 equivalent)
CL_SUPPRESS_WALL = [
    # Template/library noise (72% of warnings)
    'unused-function',                # Template functions, library functions not used
    
    # Code style warnings that are low priority for legacy codebase
    'overloaded-virtual',            # Virtual function name hiding (design issue, not bug)
    'reorder-ctor',                  # Constructor initialization order (style, not bug)
    'delete-non-abstract-non-virtual-dtor',  # Destructor design issue (legacy code)
    
    # Keep these enabled for actionable warnings:
    # 'unused-private-field',        # Unused class members (should fix)
    # 'switch',                      # Missing enum cases (should fix)
    # 'unused-value',                # Unused expression results (should fix)
    # 'sometimes-uninitialized',     # Potential uninitialized use (should fix)
    
    # Suppressed for dev preference:
    'unused-variable',             # Variables declared but not used (suppressed https://github.com/LoneGazebo/Community-Patch-DLL/pull/11826)
    'unused-but-set-variable',     # Variables set but not read (suppressed https://github.com/LoneGazebo/Community-Patch-DLL/pull/11826)
]

# Note: Third-party header warnings (Windows SDK/VC9) are suppressed via -isystem
# This preserves important project code warnings while eliminating noise from system headers

PCH_CPP = 'CvGameCoreDLL_Expansion2/_precompile.cpp'
PCH_H = 'CvGameCoreDLLPCH.h'
PCH = 'CvGameCoreDLLPCH.pch'
CPP = [
    'CvGameCoreDLL_Expansion2/Lua/CvLuaArea.cpp',
    'CvGameCoreDLL_Expansion2/Lua/CvLuaArgsHandle.cpp',
    'CvGameCoreDLL_Expansion2/Lua/CvLuaCity.cpp',
    'CvGameCoreDLL_Expansion2/Lua/CvLuaDeal.cpp',
    'CvGameCoreDLL_Expansion2/Lua/CvLuaEnums.cpp',
    'CvGameCoreDLL_Expansion2/Lua/CvLuaFractal.cpp',
    'CvGameCoreDLL_Expansion2/Lua/CvLuaGame.cpp',
    'CvGameCoreDLL_Expansion2/Lua/CvLuaGameInfo.cpp',
    'CvGameCoreDLL_Expansion2/Lua/CvLuaLeague.cpp',
    'CvGameCoreDLL_Expansion2/Lua/CvLuaMap.cpp',
    'CvGameCoreDLL_Expansion2/Lua/CvLuaPlayer.cpp',
    'CvGameCoreDLL_Expansion2/Lua/CvLuaPlot.cpp',
    'CvGameCoreDLL_Expansion2/Lua/CvLuaSupport.cpp',
    'CvGameCoreDLL_Expansion2/Lua/CvLuaTeam.cpp',
    'CvGameCoreDLL_Expansion2/Lua/CvLuaTeamTech.cpp',
    'CvGameCoreDLL_Expansion2/Lua/CvLuaUnit.cpp',
    'CvGameCoreDLL_Expansion2/CustomMods.cpp',
    'CvGameCoreDLL_Expansion2/CvAchievementInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvAchievementUnlocker.cpp',
    'CvGameCoreDLL_Expansion2/CvAdvisorCounsel.cpp',
    'CvGameCoreDLL_Expansion2/CvAdvisorRecommender.cpp',
    'CvGameCoreDLL_Expansion2/CvAIOperation.cpp',
    'CvGameCoreDLL_Expansion2/CvArea.cpp',
    'CvGameCoreDLL_Expansion2/CvArmyAI.cpp',
    'CvGameCoreDLL_Expansion2/CvAStar.cpp',
    'CvGameCoreDLL_Expansion2/CvAStarNode.cpp',
    'CvGameCoreDLL_Expansion2/CvBarbarians.cpp',
    'CvGameCoreDLL_Expansion2/CvBeliefClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvBuilderTaskingAI.cpp',
    'CvGameCoreDLL_Expansion2/CvBuildingClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvBuildingProductionAI.cpp',
    'CvGameCoreDLL_Expansion2/CvCity.cpp',
    'CvGameCoreDLL_Expansion2/CvCityAI.cpp',
    'CvGameCoreDLL_Expansion2/CvCityCitizens.cpp',
    'CvGameCoreDLL_Expansion2/CvCityConnections.cpp',
    'CvGameCoreDLL_Expansion2/CvCityManager.cpp',
    'CvGameCoreDLL_Expansion2/CvCitySpecializationAI.cpp',
    'CvGameCoreDLL_Expansion2/CvCityStrategyAI.cpp',
    'CvGameCoreDLL_Expansion2/CvContractClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvCorporationClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvCultureClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvDangerPlots.cpp',
    'CvGameCoreDLL_Expansion2/CvDatabaseUtility.cpp',
    'CvGameCoreDLL_Expansion2/CvDealAI.cpp',
    'CvGameCoreDLL_Expansion2/CvDealClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvDiplomacyAI.cpp',
    'CvGameCoreDLL_Expansion2/CvDiplomacyRequests.cpp',
    'CvGameCoreDLL_Expansion2/CvDistanceMap.cpp',
    'CvGameCoreDLL_Expansion2/CvDllBuildInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllBuildingInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllCity.cpp',
    'CvGameCoreDLL_Expansion2/CvDllCivilizationInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllColorInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllCombatInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllContext.cpp',
    'CvGameCoreDLL_Expansion2/CvDllDatabaseUtility.cpp',
    'CvGameCoreDLL_Expansion2/CvDllDeal.cpp',
    'CvGameCoreDLL_Expansion2/CvDllDealAI.cpp',
    'CvGameCoreDLL_Expansion2/CvDllDiplomacyAI.cpp',
    'CvGameCoreDLL_Expansion2/CvDllDlcPackageInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllEraInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllFeatureInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllGame.cpp',
    'CvGameCoreDLL_Expansion2/CvDllGameAsynch.cpp',
    'CvGameCoreDLL_Expansion2/CvDllGameDeals.cpp',
    'CvGameCoreDLL_Expansion2/CvDllGameOptionInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllGameSpeedInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllHandicapInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllImprovementInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllInterfaceModeInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllLeaderheadInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllMap.cpp',
    'CvGameCoreDLL_Expansion2/CvDllMinorCivInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllMissionData.cpp',
    'CvGameCoreDLL_Expansion2/CvDllMissionInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllNetInitInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllNetLoadGameInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllNetMessageExt.cpp',
    'CvGameCoreDLL_Expansion2/CvDllNetMessageHandler.cpp',
    'CvGameCoreDLL_Expansion2/CvDllNetworkSyncronization.cpp',
    'CvGameCoreDLL_Expansion2/CvDllPathFinderUpdate.cpp',
    'CvGameCoreDLL_Expansion2/CvDllPlayer.cpp',
    'CvGameCoreDLL_Expansion2/CvDllPlayerColorInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllPlayerOptionInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllPlot.cpp',
    'CvGameCoreDLL_Expansion2/CvDllPolicyInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllPreGame.cpp',
    'CvGameCoreDLL_Expansion2/CvDllPromotionInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllRandom.cpp',
    'CvGameCoreDLL_Expansion2/CvDllResourceInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllScriptSystemUtility.cpp',
    'CvGameCoreDLL_Expansion2/CvDllTeam.cpp',
    'CvGameCoreDLL_Expansion2/CvDllTechInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllTerrainInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllUnit.cpp',
    'CvGameCoreDLL_Expansion2/CvDllUnitCombatClassInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllUnitInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllVictoryInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvDllWorldBuilderMapLoader.cpp',
    'CvGameCoreDLL_Expansion2/CvDllWorldInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvEconomicAI.cpp',
    'CvGameCoreDLL_Expansion2/CvEmphasisClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvEspionageClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvEventLog.cpp',
    'CvGameCoreDLL_Expansion2/CvFlavorManager.cpp',
    'CvGameCoreDLL_Expansion2/CvFractal.cpp',
    'CvGameCoreDLL_Expansion2/CvGame.cpp',
    'CvGameCoreDLL_Expansion2/CvGameCoreDLL.cpp',
    'CvGameCoreDLL_Expansion2/CvGameCoreEnumSerialization.cpp',
    'CvGameCoreDLL_Expansion2/CvGameCoreStructs.cpp',
    'CvGameCoreDLL_Expansion2/CvGameCoreUtils.cpp',
    'CvGameCoreDLL_Expansion2/CvGameQueries.cpp',
    'CvGameCoreDLL_Expansion2/CvGameTextMgr.cpp',
    'CvGameCoreDLL_Expansion2/CvGlobals.cpp',
    'CvGameCoreDLL_Expansion2/CvGoodyHuts.cpp',
    'CvGameCoreDLL_Expansion2/CvGrandStrategyAI.cpp',
    'CvGameCoreDLL_Expansion2/CvGreatPersonInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvHomelandAI.cpp',
    'CvGameCoreDLL_Expansion2/CvImprovementClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvInfos.cpp',
    'CvGameCoreDLL_Expansion2/CvInfosSerializationHelper.cpp',
    'CvGameCoreDLL_Expansion2/CvInternalGameCoreUtils.cpp',
    'CvGameCoreDLL_Expansion2/CvLoggerCSV.cpp',
    'CvGameCoreDLL_Expansion2/CvMap.cpp',
    'CvGameCoreDLL_Expansion2/CvMapGenerator.cpp',
    'CvGameCoreDLL_Expansion2/CvMilitaryAI.cpp',
    'CvGameCoreDLL_Expansion2/CvMinorCivAI.cpp',
    'CvGameCoreDLL_Expansion2/CvNotificationClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvNotifications.cpp',
    'CvGameCoreDLL_Expansion2/CvPlayer.cpp',
    'CvGameCoreDLL_Expansion2/CvPlayerAI.cpp',
    'CvGameCoreDLL_Expansion2/CvPlayerManager.cpp',
    'CvGameCoreDLL_Expansion2/CvPlot.cpp',
    'CvGameCoreDLL_Expansion2/CvPlotInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvPlotManager.cpp',
    'CvGameCoreDLL_Expansion2/CvPolicyAI.cpp',
    'CvGameCoreDLL_Expansion2/CvPolicyClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvPopupInfoSerialization.cpp',
    'CvGameCoreDLL_Expansion2/CvPreGame.cpp',
    'CvGameCoreDLL_Expansion2/CvProcessProductionAI.cpp',
    'CvGameCoreDLL_Expansion2/CvProjectClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvProjectProductionAI.cpp',
    'CvGameCoreDLL_Expansion2/CvPromotionClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvRandom.cpp',
    'CvGameCoreDLL_Expansion2/CvReligionClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvReplayInfo.cpp',
    'CvGameCoreDLL_Expansion2/CvReplayMessage.cpp',
    'CvGameCoreDLL_Expansion2/CvSerialize.cpp',
    'CvGameCoreDLL_Expansion2/CvSiteEvaluationClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvStartPositioner.cpp',
    'CvGameCoreDLL_Expansion2/cvStopWatch.cpp',
    'CvGameCoreDLL_Expansion2/CvTacticalAI.cpp',
    'CvGameCoreDLL_Expansion2/CvTacticalAnalysisMap.cpp',
    'CvGameCoreDLL_Expansion2/CvTargeting.cpp',
    'CvGameCoreDLL_Expansion2/CvTeam.cpp',
    'CvGameCoreDLL_Expansion2/CvTechAI.cpp',
    'CvGameCoreDLL_Expansion2/CvTechClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvTradeClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvTraitClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvTreasury.cpp',
    'CvGameCoreDLL_Expansion2/CvTypes.cpp',
    'CvGameCoreDLL_Expansion2/CvUnit.cpp',
    'CvGameCoreDLL_Expansion2/CvUnitClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvUnitCombat.cpp',
    'CvGameCoreDLL_Expansion2/CvUnitCycler.cpp',
    'CvGameCoreDLL_Expansion2/CvUnitMission.cpp',
    'CvGameCoreDLL_Expansion2/CvUnitMovement.cpp',
    'CvGameCoreDLL_Expansion2/CvUnitProductionAI.cpp',
    'CvGameCoreDLL_Expansion2/CvVotingClasses.cpp',
    'CvGameCoreDLL_Expansion2/CvWonderProductionAI.cpp',
    'CvGameCoreDLL_Expansion2/CvWorldBuilderMapLoader.cpp',
]

class TaskResult:
    commands: typing.Union[str, list[str]]
    returncode: int
    
    def __init__(self, commands: typing.Union[str, list[str]]):
        self.commands = commands
        self.returncode = None

class Task:
    proc: subprocess.Popen
    result: TaskResult
    def __init__(self, commands: typing.Union[str, list[str]], env: typing.Optional[dict[str, str]]=None, shell: bool=False, log:any =None):
        self.proc = subprocess.Popen(commands, stdout=log, stderr=log, env=env, shell=shell)
        self.result = TaskResult(commands)

    def poll(self) -> typing.Optional[TaskResult]:
        if (returncode := self.proc.poll()) != None:
            self.result.returncode = returncode
            return self.result
        else:
            return None

class TaskMan:
    pending: Queue
    def __init__(self):
        self.pending = Queue()

    def spawn(self, commands: typing.Union[str, list[str]], env: typing.Optional[dict[str, str]]=None, shell: bool=False, log:any =None):
        task = Task(commands, env=env, shell=shell, log=log)
        self.pending.put(task)

    def wait(self) -> list[TaskResult]:
        results: list[TaskResult] = []
        while not self.pending.empty():
            task = self.pending.get()
            if result := task.poll():
                results.append(result)
            else:
                self.pending.put(task)
        return results
        
def set_environment(sdk_version: str):
    # For cross-compilation on Linux, we don't need to set Windows-specific environment
    # The include and library paths are handled via clang arguments
    print(f"Cross-compiling for Windows using SDK version {sdk_version}")

def print_environment():
    print("Cross-compilation environment:")
    print("Target:", "i686-pc-windows-msvc")
    print("SDK Version:", SDK_VERSION)
    print("Include paths:", INCLUDE_PATHS)
    print("Library paths:", LIB_PATHS)

def build_cl_config_args(config: Config) -> list[str]:
    # Convert MSVC-style flags to clang-compatible flags for cross-compilation
    args = [
        '-target', 'i686-pc-windows-msvc',
        '-msse3',
        '-c',  # Compile only, don't link
        '-fexceptions',  # Equivalent to /EHsc
        '-g',  # Debug info (equivalent to /Z7)
        '-fms-extensions',  # Enable Microsoft extensions
        '-fms-compatibility',  # MSVC compatibility mode
        '-fdelayed-template-parsing',  # MSVC template parsing
        '-D_DLL',  # Define _DLL for dynamic runtime (equivalent to /MD)
        '-D_MT',   # Define _MT for multithreaded runtime

    ]
    
    if config == Config.Release:
        args.extend(['-O2', '-DNDEBUG'])  # Equivalent to /Ox, /Ob2
    else:
        args.extend(['-O0', '-DDEBUG'])  # Equivalent to /Od
    
    # Add preprocessor definitions
    for predef in PREDEFS[config]:
        args.append(f'-D{predef}')
    
    # Prevent DriverSpecs.h from being included to avoid SAL token pasting issues
    # This is the cleanest solution - prevent the problematic header entirely
    args.append('-DDRIVERSPECS_H')  # Define the include guard to prevent inclusion
    
    # Force VC9 runtime compatibility  
    args.append('-U_MSC_VER')       # Undefine clang's version
    args.append('-D_MSC_VER=1500')  # Force VC9 version (VS2008)
    args.append('-D_WIN32_WINNT=0x0501')  # Windows XP compatibility
    
    # Include compatibility header for clang cross-compilation on Linux
    args.append('-include')
    args.append(os.path.join(PROJECT_DIR, 'clang_linux_compat.h'))
    
    # Add include directories (project headers)
    for include_dir in INCLUDE_DIRS:
        args.append(f'-I{os.path.join(PROJECT_DIR, include_dir)}')
    
    # Add third-party library headers as system headers (suppress warnings)
    for include_dir in SYSTEM_INCLUDE_DIRS:
        args.append(f'-isystem{os.path.join(PROJECT_DIR, include_dir)}')
    
    # Add external include paths as system headers (Windows SDK and VC includes)
    # Using -isystem suppresses warnings from these third-party headers
    for include_path in INCLUDE_PATHS:
        args.append(f'-isystem{include_path}')
    
    # Add -Wall for comprehensive warning coverage
    args.append('-Wall')
    args.append('-Wextra')
    
    # Enhanced warnings to catch undefined behavior and potential bugs
    # These are C++03/TR1 compatible and focus on runtime safety
    enhanced_warnings = [
        # Undefined behavior detection (compile-time)
        '-Warray-bounds',                    # Array bounds checking
        '-Wshift-count-overflow',            # Shift count >= width of type
        '-Wshift-count-negative',            # Negative shift count
        '-Wshift-overflow',                  # Left shift overflow
        '-Wdivision-by-zero',               # Division by zero (compile-time detectable)
        '-Winteger-overflow',               # Integer overflow in expressions
        '-Wbool-operation',                 # Suspicious operations on bool
        '-Wlogical-op-parentheses',         # Logical operator precedence issues
        '-Wbitwise-op-parentheses',         # Bitwise operator precedence issues
        '-Wdangling-else',                  # Ambiguous else clauses
        
        # Memory safety warnings
        '-Wuninitialized',                  # Use of uninitialized variables
        '-Wconditional-uninitialized',      # Conditionally uninitialized variables
        '-Wreturn-stack-address',           # Returning address of local variable
        '-Wdangling-field',                 # Dangling references in fields
        '-Wself-assign',                    # Self assignment (x = x)
        '-Wself-move',                      # Self move (C++11, but harmless check)
        
        # Type safety and conversion warnings
        '-Wfloat-conversion',               # Implicit float conversions that lose precision
        '-Wshorten-64-to-32',              # 64-bit to 32-bit narrowing
        '-Wbool-conversion',               # Implicit conversions to bool
        '-Wenum-conversion',               # Implicit enum conversions
        '-Wstring-conversion',             # String literal to bool conversion
        '-Wpointer-arith',                 # Pointer arithmetic on void*/function pointers
        '-Wcast-align',                    # Cast increases required alignment
        
        # Control flow warnings
        '-Wunreachable-code',              # Unreachable code detection
        '-Wmissing-noreturn',              # Functions that should be marked noreturn
        '-Winfinite-recursion',            # Infinite recursion detection
        '-Wfor-loop-analysis',             # Suspicious for loop conditions
        
        # Function call safety
        '-Wformat-security',               # Format string security issues
        '-Wformat-nonliteral',             # Non-literal format strings
        '-Wnonnull',                       # NULL passed to nonnull parameter
        '-Wreturn-type',                   # Missing return statements
        
        # C++03/TR1 specific safety checks
        '-Woverloaded-virtual',            # Virtual function hiding (re-enable, it's important)
        # '-Wvirtual-dtor',                # Missing virtual destructors (not supported in this clang version)
        '-Wnon-virtual-dtor',              # Non-virtual destructors in base classes
        '-Wdelete-non-virtual-dtor',       # Delete through non-virtual destructor
    ]
    
    for warning in enhanced_warnings:
        args.append(warning)
    
    # Suppress specific warnings
    for suppress in CL_SUPPRESS:
        args.append(f'-Wno-{suppress}')
    
    # Suppress additional -Wall warnings for practical use
    # Skip overloaded-virtual since we want the enhanced version enabled
    for suppress in CL_SUPPRESS_WALL:
        if suppress != 'overloaded-virtual':  # Let enhanced warnings handle this
            args.append(f'-Wno-{suppress}')
    
    # Suppress enhanced warnings that generate too much noise from legacy/interface code
    enhanced_suppressions = [
        'non-virtual-dtor',              # Interface classes with virtual functions but non-virtual destructors
        'delete-non-virtual-dtor',       # Related to above - legacy interface design
        'cast-align',                    # Alignment warnings from third-party libraries (should be reduced by -isystem)
        'missing-noreturn',              # Functions that could be marked noreturn (low priority)
        'unreachable-code',              # Dead code (should be cleaned up but not critical)
    ]
    
    for suppress in enhanced_suppressions:
        args.append(f'-Wno-{suppress}')
    
    return args

def build_link_config_args(config: Config) -> list[str]:
    args = ['/MACHINE:x86', '/DLL', '/DEBUG', '/LTCG', '/DYNAMICBASE', '/NXCOMPAT', '/SUBSYSTEM:WINDOWS', '/MANIFEST:EMBED', '/FORCE:MULTIPLE', f'/DEF:"{os.path.join(PROJECT_DIR, DEF_FILE)}"']
    if config == Config.Release:
        args += ['/OPT:REF', '/OPT:ICF']
    return args

def prepare_dirs(build_dir: Path, out_dir: Path):
    build_dir.mkdir(parents=True, exist_ok=True)
    out_dir.mkdir(parents=True, exist_ok=True)
    for cpp in CPP:
        cpp_dir = build_dir.joinpath(Path(cpp).parent)
        cpp_dir.mkdir(parents=True, exist_ok=True)

def build_clang_cpp(cl: str, cl_args: list[str], build_dir: Path, log: typing.IO):
    print('building clang.cpp...')
    start_time = time.time()
    src = PROJECT_DIR.joinpath('clang.cpp')
    out = build_dir.joinpath('clang.obj')
    command = [cl, str(src), '-o', str(out)] + cl_args
    cp = subprocess.run(command, capture_output=True)
    log.write(str.encode(f'==== {src} ====\n'))
    log.write(cp.stdout)
    log.write(cp.stderr)
    log.flush()
    if cp.returncode != 0:
        print('failed to build clang.cpp - see build log')
        quit()
    end_time = time.time()
    print(f'clang.cpp build finished after {end_time - start_time} seconds')

def build_threading_stub(cl: str, build_dir: Path, log: typing.IO):
    print('building threading stub...')
    start_time = time.time()
    src = PROJECT_DIR.joinpath('clang_linux_threading.cpp')
    out = build_dir.joinpath('clang_linux_threading.obj')
    # Use minimal compilation flags for the stub
    command = [cl, '--target=i686-pc-windows-msvc', '-c', str(src), '-o', str(out)]
    cp = subprocess.run(command, capture_output=True)
    log.write(str.encode(f'==== {src} ====\n'))
    log.write(cp.stdout)
    log.write(cp.stderr)
    log.flush()
    if cp.returncode != 0:
        print('failed to build threading stub - see build log')
        quit()
    end_time = time.time()
    print(f'threading stub build finished after {end_time - start_time} seconds')

def update_commit_id(log: typing.IO):
    print('updating commit id...')
    start_time = time.time()
    
    try:
        # Check git status
        status_result = subprocess.run(['git', 'status', '--untracked-files=no', '--porcelain'], 
                                     capture_output=True, text=True)
        status = "Dirty" if status_result.stdout.strip() else "Clean"
        
        # Get tag and commit info
        tag_result = subprocess.run(['git', 'describe', '--abbrev=0'], 
                                  capture_output=True, text=True)
        tag = tag_result.stdout.strip() if tag_result.returncode == 0 else "unknown"
        
        head_result = subprocess.run(['git', 'rev-list', '--abbrev-commit', '-n', '1', 'HEAD'], 
                                   capture_output=True, text=True)
        head_commit = head_result.stdout.strip() if head_result.returncode == 0 else "unknown"
        
        tag_commit_result = subprocess.run(['git', 'rev-list', '--abbrev-commit', '-n', '1', tag], 
                                         capture_output=True, text=True)
        tag_commit = tag_commit_result.stdout.strip() if tag_commit_result.returncode == 0 else "unknown"
        
        # Generate version string
        if head_commit == tag_commit:
            version_str = f"{tag} {status}"
        else:
            version_str = f"{tag} {head_commit} {status}"
        
        # Write commit_id.inc file
        commit_id_content = f'const char CURRENT_GAMECORE_VERSION[] = "{version_str}"; //autogenerated, do not commit this file!\n'
        with open(PROJECT_DIR / 'commit_id.inc', 'w') as f:
            f.write(commit_id_content)
        
        log.write(f'==== update_commit_id (Linux) ====\n'.encode())
        log.write(f'Version identifier will be "{version_str}"\n'.encode())
        log.flush()
        
        print(f'Version identifier will be "{version_str}"')
        
    except Exception as e:
        log.write(f'Error updating commit id: {str(e)}\n'.encode())
        print(f'Warning: failed to update commit id - {str(e)}')
    
    end_time = time.time()
    print(f'commit id update finished after {end_time - start_time} seconds')

def build_pch(cl: str, cl_args: list[str], pch_path: Path, build_dir: Path, log: typing.IO):
    print('building precompiled header...')
    start_time = time.time()
    pch_src = PROJECT_DIR.joinpath(PCH_CPP)
    out = build_dir.joinpath(PCH_CPP).with_suffix('.obj')
    # For clang, we need to use different PCH flags
    # We'll skip PCH for now and just compile normally
    command = [cl, str(pch_src), '-o', str(out)] + cl_args
    cp = subprocess.run(command, capture_output=True)
    log.write(str.encode(f'==== {pch_src} ====\n'))
    log.write(cp.stdout)
    log.write(cp.stderr)
    log.flush()
    if cp.returncode != 0:
        print('failed to build precompiled header - see build log')
        quit()
    end_time = time.time()
    print(f'precompiled header build finished after {end_time - start_time} seconds')

def build_cpps(cl: str, cl_args: list[str], pch_path: Path, build_dir: Path, log: typing.IO):
    print('building cpps...')
    start_time = time.time()
    build_tasks = TaskMan()
    logs: dict[Path, typing.IO] = {}
    try:
        for cpp in CPP:
            cpp_src = PROJECT_DIR.joinpath(cpp)
            cpp_log = tempfile.TemporaryFile()
            logs[cpp_src] = cpp_log
            out = build_dir.joinpath(cpp).with_suffix('.obj')
            # Skip PCH for clang cross-compilation for now
            command = [cl, str(cpp_src), '-o', str(out)] + cl_args
            build_tasks.spawn(command, log=cpp_log)
        build_results = build_tasks.wait()
        for cpp_src, cpp_log in logs.items():
            cpp_log.seek(0, 0)
            contents = cpp_log.read()
            log.write(str.encode(f'==== {cpp_src} ====\n'))
            log.write(contents)
            del cpp_log
        log.flush()
        failed = 0
        for result in build_results:
            if result.returncode != 0:
                failed += 1
        if failed != 0:
            print(f'{failed} cpp(s) failed to build - see build log')
            quit()
        end_time = time.time()
        print(f'cpps build finished after {end_time - start_time} seconds')
    finally:
       del logs

def link_dll(link: str, link_args: list[str], build_dir: Path, out_dir: Path, log: typing.IO):
    print('linking dll...')
    start_time = time.time()
    out_dir.mkdir(parents=True, exist_ok=True)
    link_response_file_name = build_dir.joinpath('link.rsp')
    with open(link_response_file_name, 'w') as link_response_file:
        out_dll = out_dir.joinpath(f'{CORE_DLL}.dll')
        out_pdb = out_dir.joinpath(f'{CORE_DLL}.pdb')
        link_response_file.write(f'/OUT:"{out_dll}"\n/PDB:"{out_pdb}"\n')
        link_response_file.write('\n'.join(link_args) + '\n')
        for lib_path in LIB_PATHS:
            link_response_file.write(f'/LIBPATH:"{lib_path}"\n')
        for lib in LIBS:
            lib_path = PROJECT_DIR.joinpath(lib)
            if not lib_path.exists():
                log.write(f'Warning: Library file "{lib_path}" does not exist.\n'.encode())
            link_response_file.write(f'"{lib_path}"\n')
        for default_lib in DEFAULT_LIBS:
            link_response_file.write(f'{default_lib}\n')
        clang_obj = build_dir.joinpath('clang.obj')
        threading_obj = build_dir.joinpath('clang_linux_threading.obj')
        pch_obj = build_dir.joinpath(PCH_CPP).with_suffix('.obj')
        link_response_file.write(f'"{clang_obj}"\n"{threading_obj}"\n"{pch_obj}"\n')
        for cpp in CPP:
            cpp_obj = build_dir.joinpath(cpp).with_suffix('.obj')
            if not cpp_obj.exists():
                log.write(f'Warning: Object file "{cpp_obj}" does not exist.\n'.encode())
            link_response_file.write(f'"{cpp_obj}"\n')
    command = [link, f'@{link_response_file_name}', '/force:multiple', '/force:unresolved']
    log.write(f'Linking command: {" ".join(command)}\n'.encode())
    cp = subprocess.run(command, capture_output=True)
    log.write(str.encode(f'==== {CORE_DLL}.dll ====\n'))
    log.write(cp.stdout)
    log.write(cp.stderr)
    log.flush()
    end_time = time.time()
    if cp.returncode != 0:
        print('linking dll failed - see build log')
        log.write(f'Linking failed with return code {cp.returncode}\n'.encode())
        quit()
    print(f'linking dll finished after {end_time - start_time} seconds')

set_environment(SDK_VERSION)
print_environment()

arg_parser = argparse.ArgumentParser(description='Build VP.')
arg_parser.add_argument('--config', type=str, default='debug', choices=['release', 'debug'])
args = arg_parser.parse_args()
config = Config.Release if args.config == 'release' else Config.Debug

cl = 'clang'  # Use clang instead of clang-cl for Linux
link = 'lld-link'  # Remove .exe extension for Linux
build_dir = PROJECT_DIR.joinpath(BUILD_DIR[config])
out_dir = PROJECT_DIR.joinpath(PROJECT_DIR, OUT_DIR[config])
cl_args = build_cl_config_args(config)
link_args = build_link_config_args(config)
pch_path = os.path.join(build_dir, PCH)
prepare_dirs(build_dir, out_dir)

log = open(out_dir.joinpath('build.log'), mode='w+b')
try:
    update_commit_id(log)
    build_clang_cpp(cl, cl_args, build_dir, log)
    build_threading_stub(cl, build_dir, log)
    build_pch(cl, cl_args, pch_path, build_dir, log)
    build_cpps(cl, cl_args, pch_path, build_dir, log)
    link_dll(link, link_args, build_dir, out_dir, log)
finally:
    log.close()