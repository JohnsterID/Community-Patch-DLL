import os
import subprocess
from enum import Enum
import typing
import time
import tempfile
from pathlib import Path
import argparse
from queue import Queue
import sys

class Config(Enum):
    Release = 0
    Debug = 1

class Sanitizer(Enum):
    NONE = 0
    UBSAN = 1
    ASAN = 2

# 32-bit LLVM path for sanitizer builds (UBSan and ASan)
LLVM_PATH = Path(r'C:\Program Files (x86)\LLVM\bin')
# ASan runtime DLL produced by the 32-bit LLVM toolchain; must sit next to the
# game DLL at runtime so Windows can load it.
ASAN_RUNTIME_DLL = 'clang_rt.asan_dynamic-i386.dll'
# Fixed preferred base address for the ASan runtime copy placed in the output
# directory.  Without this, ASLR can land the DLL anywhere in the 32-bit
# address space, including the ASan shadow range [0x30000000-0x4fffffff],
# which causes __asan_init() to abort on the very first LoadLibraryW call.
# 0x72000000 is in HighMem (>= 0x50000000), well above all shadow regions,
# and matches where ASLR placed the DLL in all previously successful sessions.
ASAN_REBASE_ADDRESS = '0x72000000'
# Shadow-range pre-reservation shim built alongside clang_rt.asan_dynamic-i386.dll.
# Injected into CivilizationV.exe at process start by asan_launcher.exe.
SHADOW_BOOT_DLL = 'asan_shadow_boot.dll'
SHADOW_BOOT_SRC = Path('asan_shadow_boot') / 'asan_shadow_boot.c'
# Launcher EXE: suspends CivilizationV.exe, injects the boot DLL, then resumes.
# Replaces AppInit_DLLs; no registry changes needed.
ASAN_LAUNCHER_EXE = 'asan_launcher.exe'
ASAN_LAUNCHER_SRC = Path('asan_launcher') / 'asan_launcher.c'

VS_2008_VARS_BAT = Path(os.environ['VS90COMNTOOLS']).joinpath('vsvars32.bat')
CORE_DLL = 'CvGameCore_Expansion2'
PROJECT_DIR = Path().resolve()
BUILD_DIR = {
    Config.Release: 'clang-build\\Release',
    Config.Debug: 'clang-build\\Debug',
}
OUT_DIR = {
    Config.Release: 'clang-output\\Release',
    Config.Debug: 'clang-output\\Debug',
}
LIBS = [
    'CvWorldBuilderMap\\lib\\CvWorldBuilderMapWin32.obj',
    'CvGameCoreDLLUtil\\lib\\CvGameCoreDLLUtilWin32.lib',
    'CvLocalization\\lib\\CvLocalizationWin32.lib',
    'CvGameDatabase\\lib\\CvGameDatabaseWin32.lib',
    'FirePlace\\lib\\FireWorksWin32.obj',
    'FirePlace\\lib\\FLuaWin32.lib',
    'ThirdPartyLibs\\Lua51\\lib\\lua51_Win32.lib',
]
DEFAULT_LIBS = [
    'winmm.lib',
    'kernel32.lib',
    'user32.lib',
    'gdi32.lib',
    'winspool.lib',
    'comdlg32.lib',
    'advapi32.lib',
    'shell32.lib',
    'ole32.lib',
    'oleaut32.lib',
    'uuid.lib',
    'odbc32.lib',
    'odbccp32.lib',
    'dbghelp.lib',     # CaptureStackBackTrace + SymFromAddr for UBSan stack traces
]
DEF_FILE = 'CvGameCoreDLL_Expansion2\\CvGameCoreDLL.def'
INCLUDE_DIRS = [
    'CvGameCoreDLL_Expansion2',
    'CvWorldBuilderMap\\include',
    'CvGameCoreDLLUtil\\include',
    'CvLocalization\\include',
    'CvGameDatabase\\include',
    'FirePlace\\include',
    'FirePlace\\include\\FireWorks',
    'ThirdPartyLibs\\Lua51\\include'
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
]
RELEASE_PREDEFS = SHARED_PREDEFS + ['STRONG_ASSUMPTIONS', 'NDEBUG', 'VPRELEASE_ERRORMSG']
DEBUG_PREDEFS = SHARED_PREDEFS + ['VPDEBUG']
PREDEFS = {
    Config.Release: RELEASE_PREDEFS,
    Config.Debug: DEBUG_PREDEFS,
}
CL_SUPPRESS = [
    'invalid-offsetof',
    'tautological-constant-out-of-range-compare',
    'comment',
    'enum-constexpr-conversion', # TODO: #9786
]
PCH_CPP = 'CvGameCoreDLL_Expansion2\\_precompile.cpp'
PCH_H = 'CvGameCoreDLLPCH.h'
PCH = 'CvGameCoreDLLPCH.pch'
CPP = [
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaArea.cpp',
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaArgsHandle.cpp',
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaCity.cpp',
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaDeal.cpp',
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaEnums.cpp',
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaFractal.cpp',
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaGame.cpp',
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaGameInfo.cpp',
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaLeague.cpp',
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaMap.cpp',
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaPlayer.cpp',
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaPlot.cpp',
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaSupport.cpp',
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaTeam.cpp',
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaTeamTech.cpp',
    'CvGameCoreDLL_Expansion2\\Lua\\CvLuaUnit.cpp',
    'CvGameCoreDLL_Expansion2\\CustomMods.cpp',
    'CvGameCoreDLL_Expansion2\\CvAchievementInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvAchievementUnlocker.cpp',
    'CvGameCoreDLL_Expansion2\\CvAdvisorCounsel.cpp',
    'CvGameCoreDLL_Expansion2\\CvAdvisorRecommender.cpp',
    'CvGameCoreDLL_Expansion2\\CvAIOperation.cpp',
    'CvGameCoreDLL_Expansion2\\CvArea.cpp',
    'CvGameCoreDLL_Expansion2\\CvArmyAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvAStar.cpp',
    'CvGameCoreDLL_Expansion2\\CvAStarNode.cpp',
    'CvGameCoreDLL_Expansion2\\CvBarbarians.cpp',
    'CvGameCoreDLL_Expansion2\\CvBeliefClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvBuilderTaskingAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvBuildingClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvBuildingProductionAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvCity.cpp',
    'CvGameCoreDLL_Expansion2\\CvCityAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvCityCitizens.cpp',
    'CvGameCoreDLL_Expansion2\\CvCityConnections.cpp',
    'CvGameCoreDLL_Expansion2\\CvCityManager.cpp',
    'CvGameCoreDLL_Expansion2\\CvCitySpecializationAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvCityStrategyAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvContractClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvCorporationClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvCultureClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvDangerPlots.cpp',
    'CvGameCoreDLL_Expansion2\\CvDatabaseUtility.cpp',
    'CvGameCoreDLL_Expansion2\\CvDealAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvDealClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvDiplomacyAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvDiplomacyRequests.cpp',
    'CvGameCoreDLL_Expansion2\\CvDistanceMap.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllBuildInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllBuildingInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllCity.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllCivilizationInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllColorInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllCombatInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllContext.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllDatabaseUtility.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllDeal.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllDealAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllDiplomacyAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllDlcPackageInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllEraInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllFeatureInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllGame.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllGameAsynch.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllGameDeals.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllGameOptionInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllGameSpeedInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllHandicapInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllImprovementInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllInterfaceModeInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllLeaderheadInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllMap.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllMinorCivInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllMissionData.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllMissionInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllNetInitInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllNetLoadGameInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllNetMessageExt.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllNetMessageHandler.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllNetworkSyncronization.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllPathFinderUpdate.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllPlayer.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllPlayerColorInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllPlayerOptionInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllPlot.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllPolicyInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllPreGame.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllPromotionInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllRandom.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllResourceInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllScriptSystemUtility.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllTeam.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllTechInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllTerrainInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllUnit.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllUnitCombatClassInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllUnitInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllVictoryInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllWorldBuilderMapLoader.cpp',
    'CvGameCoreDLL_Expansion2\\CvDllWorldInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvEconomicAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvEmphasisClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvEspionageClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvEventLog.cpp',
    'CvGameCoreDLL_Expansion2\\CvFlavorManager.cpp',
    'CvGameCoreDLL_Expansion2\\CvFractal.cpp',
    'CvGameCoreDLL_Expansion2\\CvGame.cpp',
    'CvGameCoreDLL_Expansion2\\CvGameCoreDLL.cpp',
    'CvGameCoreDLL_Expansion2\\asan_compat.cpp',
    'CvGameCoreDLL_Expansion2\\CvGameCoreEnumSerialization.cpp',
    'CvGameCoreDLL_Expansion2\\CvGameCoreStructs.cpp',
    'CvGameCoreDLL_Expansion2\\CvGameCoreUtils.cpp',
    'CvGameCoreDLL_Expansion2\\CvGameQueries.cpp',
    'CvGameCoreDLL_Expansion2\\CvGameTextMgr.cpp',
    'CvGameCoreDLL_Expansion2\\CvGlobals.cpp',
    'CvGameCoreDLL_Expansion2\\CvGoodyHuts.cpp',
    'CvGameCoreDLL_Expansion2\\CvGrandStrategyAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvGreatPersonInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvHomelandAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvImprovementClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvInfos.cpp',
    'CvGameCoreDLL_Expansion2\\CvInfosSerializationHelper.cpp',
    'CvGameCoreDLL_Expansion2\\CvInternalGameCoreUtils.cpp',
    'CvGameCoreDLL_Expansion2\\CvLoggerCSV.cpp',
    'CvGameCoreDLL_Expansion2\\CvMap.cpp',
    'CvGameCoreDLL_Expansion2\\CvMapGenerator.cpp',
    'CvGameCoreDLL_Expansion2\\CvMilitaryAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvMinorCivAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvNotificationClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvNotifications.cpp',
    'CvGameCoreDLL_Expansion2\\CvPlayer.cpp',
    'CvGameCoreDLL_Expansion2\\CvPlayerAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvPlayerManager.cpp',
    'CvGameCoreDLL_Expansion2\\CvPlot.cpp',
    'CvGameCoreDLL_Expansion2\\CvPlotInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvPlotManager.cpp',
    'CvGameCoreDLL_Expansion2\\CvPolicyAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvPolicyClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvPopupInfoSerialization.cpp',
    'CvGameCoreDLL_Expansion2\\CvPreGame.cpp',
    'CvGameCoreDLL_Expansion2\\CvProcessProductionAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvProjectClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvProjectProductionAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvPromotionClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvRandom.cpp',
    'CvGameCoreDLL_Expansion2\\CvReligionClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvReplayInfo.cpp',
    'CvGameCoreDLL_Expansion2\\CvReplayMessage.cpp',
    'CvGameCoreDLL_Expansion2\\CvSerialize.cpp',
    'CvGameCoreDLL_Expansion2\\CvSiteEvaluationClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvStartPositioner.cpp',
    'CvGameCoreDLL_Expansion2\\cvStopWatch.cpp',
    'CvGameCoreDLL_Expansion2\\CvTacticalAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvTacticalAnalysisMap.cpp',
    'CvGameCoreDLL_Expansion2\\CvTargeting.cpp',
    'CvGameCoreDLL_Expansion2\\CvTeam.cpp',
    'CvGameCoreDLL_Expansion2\\CvTechAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvTechClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvTradeClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvTraitClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvTreasury.cpp',
    'CvGameCoreDLL_Expansion2\\CvTypes.cpp',
    'CvGameCoreDLL_Expansion2\\CvUnit.cpp',
    'CvGameCoreDLL_Expansion2\\CvUnitClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvUnitCombat.cpp',
    'CvGameCoreDLL_Expansion2\\CvUnitCycler.cpp',
    'CvGameCoreDLL_Expansion2\\CvUnitMission.cpp',
    'CvGameCoreDLL_Expansion2\\CvUnitMovement.cpp',
    'CvGameCoreDLL_Expansion2\\CvUnitProductionAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvVotingClasses.cpp',
    'CvGameCoreDLL_Expansion2\\CvWonderProductionAI.cpp',
    'CvGameCoreDLL_Expansion2\\CvWorldBuilderMapLoader.cpp',
    'CvGameCoreDLL_Expansion2\\ubsan_handlers.cpp',
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

def build_cl_config_args(config: Config, sanitizer: Sanitizer) -> list[str]:
    args = ['-m32', '-msse3', '/c', '/MD', '/GS', '/EHsc', '/fp:precise', '/Zc:wchar_t', '/Zi', '/FS']
    if config == Config.Release:
        args.append('/Ox')
        args.append('/Ob2')
        args.append('/Zo')
        args.append('-flto')
    else:
        args.append('/Od')
        args.append('/Oy-')
    for predef in PREDEFS[config]:
        args.append(f'/D{predef}')
    for include_dir in INCLUDE_DIRS:
        args.append(f'/I"{os.path.join(PROJECT_DIR, include_dir)}"')
    for suppress in CL_SUPPRESS:
        args.append(f'-Wno-{suppress}')
    # UBSan for Debug builds (using custom VS2008-compatible handlers in ubsan_handlers.cpp)
    if sanitizer == Sanitizer.UBSAN:
        args.append('-fsanitize=undefined')
        args.append('-fsanitize=unsigned-integer-overflow')             # not UB but catches unintentional unsigned wrapping
        args.append('-fsanitize=implicit-signed-integer-truncation')    # lossy signed narrowing (int32 -> int8 losing high bits)
        args.append('-fsanitize=implicit-unsigned-integer-truncation')  # lossy unsigned narrowing (uint32 -> uint8)
        args.append('-fsanitize=implicit-integer-sign-change')          # sign-confused assignments (large uint -> int)
        args.append('-fno-sanitize=enum')  # all Civ5 enums are "open" (database-driven values)
        args.append(f'-fsanitize-ignorelist={os.path.join(PROJECT_DIR, "ubsan.ignore")}')
    # ASan for Debug builds — uses the dynamic clang_rt.asan_dynamic-i386.dll runtime.
    # No custom handler file is needed: the runtime supplies all __asan_report_* symbols.
    #
    # DLL-only limitation: allocations made by the uninstrumented game EXE are untracked,
    # so use-after-free and overflows that straddle the EXE/DLL boundary may not be caught.
    # Everything allocated inside the DLL (heap, stack, globals) is fully instrumented.
    #
    # 32-bit address-space note: ASan reserves ~256 MB of shadow memory.  If the game
    # crashes at startup, try adding "-mllvm -asan-mapping-scale=5" to shrink the shadow.
    elif sanitizer == Sanitizer.ASAN:
        args.append('-fsanitize=address')
        args.append('-fsanitize-recover=address')   # log and continue rather than abort on first error
        # Frame pointers: clang-cl uses /Oy- (already appended above for Debug); no GCC-style flag needed.
        args.append('-mllvm')
        args.append('-asan-use-after-return=never') # skip UAR stack instrumentation; reduces shadow pressure on 32-bit
        # NOTE: -asan-mapping-scale has NO effect when using the pre-built clang_rt.asan_dynamic-i386.dll.
        # That DLL has scale=3 and shadow_offset=0x30000000 compiled-in as constants; they cannot be
        # overridden externally.  With -fsanitize=address in DLL mode clang-cl emits call-out
        # instrumentation (__asan_loadN/__asan_storeN) rather than inline shadow checks, so the
        # compiler-side scale flag is also a no-op at runtime.  The flag is kept here for
        # completeness in case the static runtime is ever used instead.
        args.append('-mllvm')
        args.append('-asan-mapping-scale=5')        # 1:32 shadow ratio — effective only with static runtime
        args.append(f'-fsanitize-ignorelist={os.path.join(PROJECT_DIR, "asan.ignore")}')
    return args

def build_link_config_args(config: Config, sanitizer: Sanitizer) -> list[str]:
    args = ['/MACHINE:x86', '/DLL', '/DEBUG', '/LTCG', '/DYNAMICBASE', '/NXCOMPAT', '/SUBSYSTEM:WINDOWS', '/MANIFEST:EMBED', '/FORCE:MULTIPLE', f'/DEF:"{os.path.join(PROJECT_DIR, DEF_FILE)}"']
    if config == Config.Release:
        args += ['/OPT:REF', '/OPT:ICF']
    # lld-link is invoked directly via a response file, so clang-cl's automatic
    # runtime-library injection does not happen.  Add the ASan import library
    # and its search path explicitly so ___asan_* symbols resolve.
    if sanitizer == Sanitizer.ASAN:
        lib_dir = find_asan_lib_dir()
        if lib_dir:
            args.append(f'/LIBPATH:"{lib_dir}"')
        args.append('/DEFAULTLIB:clang_rt.asan_dynamic-i386.lib')
    return args

def prepare_dirs(build_dir: Path, out_dir: Path):
    build_dir.mkdir(parents=True, exist_ok=True)
    out_dir.mkdir(parents=True, exist_ok=True)
    for cpp in CPP:
        cpp_dir = build_dir.joinpath(Path(cpp).parent)
        cpp_dir.mkdir(parents=True, exist_ok=True)

def build_clang_cpp(cl: str, cl_args: str, build_dir: Path, log: typing.IO):
    print('building clang.cpp...')
    start_time = time.time()
    src = PROJECT_DIR.joinpath('clang.cpp')
    out = build_dir.joinpath('clang.obj')
    command = f'"{VS_2008_VARS_BAT}">NUL && "{cl}" "{src}" /Fo"{out}" {cl_args}'
    cp = subprocess.run(command, capture_output=True)
    log.write(str.encode(f'==== {src} ====\n'))
    log.write(cp.stdout)
    log.write(cp.stderr)
    log.flush()
    if cp.returncode != 0:
        print('failed to build clang.cpp - see build log')
        sys.exit(1)
    end_time = time.time()
    print(f'clang.cpp build finished after {end_time - start_time} seconds')

def update_commit_id(log: typing.IO):
    print('updating commit id...')
    start_time = time.time()
    cp = subprocess.run('update_commit_id.bat', capture_output=True)
    log.write(str.encode(f'==== update_commit_id.bat ====\n'))
    log.write(cp.stdout)
    log.write(cp.stderr)
    log.flush()
    if cp.returncode != 0:
        print('failed to update commit id - see build log')
        sys.exit(1)
    end_time = time.time()
    print(f'commit id update finished after {end_time - start_time} seconds')

def build_pch(cl: str, cl_args: str, pch_path: Path, build_dir: Path, log: typing.IO):
    print('building precompiled header...')
    start_time = time.time()
    pch_src = PROJECT_DIR.joinpath(PCH_CPP)
    out = build_dir.joinpath(PCH_CPP).with_suffix('.obj')
    command = f'"{VS_2008_VARS_BAT}">NUL && "{cl}" "{pch_src}" /Fo"{out}" /Yc"{PCH_H}" /Fp"{pch_path}" {cl_args}'
    cp = subprocess.run(command, capture_output=True)
    log.write(str.encode(f'==== {pch_src} ====\n'))
    log.write(cp.stdout)
    log.write(cp.stderr)
    log.flush()
    if cp.returncode != 0:
        print('failed to build precompiled header - see build log')
        sys.exit(1)
    end_time = time.time()
    print(f'precompiled header build finished after {end_time - start_time} seconds')

def build_cpps(cl: str, cl_args: str, pch_path: Path, build_dir: Path, log: typing.IO):
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
            command = f'"{VS_2008_VARS_BAT}">NUL && "{cl}" "{cpp_src}" /Fo"{out}" /Yu"{PCH_H}" /Fp"{pch_path}" {cl_args}'
            build_tasks.spawn(command, log=cpp_log)
        build_results = build_tasks.wait()
        for cpp_src, cpp_log in logs.items():
            cpp_log.seek(0, 0)
            contents = cpp_log.read();
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
            sys.exit(1)
        end_time = time.time()
        print(f'cpps build finished after {end_time - start_time} seconds')
    finally:
       del logs

def link_dll(link: str, link_args: list[str], build_dir: Path, out_dir: Path, log: typing.IO):
    print('linking dll...')
    start_time = time.time()
    link_response_file_name = build_dir.joinpath('link')
    with open(link_response_file_name, 'w') as link_response_file:
        out_dll = out_dir.joinpath(f'{CORE_DLL}.dll')
        out_pdb = out_dir.joinpath(f'{CORE_DLL}.pdb')
        link_response_file.write(f'/OUT:"{out_dll}"\n/PDB:"{out_pdb}"\n')
        link_response_file.write('\n'.join(link_args))
        for lib in LIBS:
            lib_path = PROJECT_DIR.joinpath(lib)
            link_response_file.write(f'\n"{lib_path}"')
        for default_lib in DEFAULT_LIBS:
            link_response_file.write(f'\n"{default_lib}"')
        clang_obj = build_dir.joinpath('clang.obj')
        pch_obj = build_dir.joinpath(PCH_CPP).with_suffix('.obj')
        link_response_file.write(f'\n"{clang_obj}"\n"{pch_obj}"')
        for cpp in CPP:
            cpp_obj = build_dir.joinpath(cpp).with_suffix('.obj')
            link_response_file.write(f'\n"{cpp_obj}"')
        link_response_file.close()
    command = f'"{VS_2008_VARS_BAT}">NUL && "{link}" @"{link_response_file_name}"'
    cp = subprocess.run(command, capture_output=True)
    log.write(str.encode(f'==== {CORE_DLL}.dll ====\n'))
    log.write(cp.stdout)
    log.write(cp.stderr)
    log.flush()
    end_time = time.time()
    if cp.returncode != 0:
        print('linking dll failed - see build log')
        sys.exit(1)
    print(f'linking dll finished after {end_time - start_time} seconds')

def find_asan_lib_dir() -> typing.Optional[Path]:
    """Return the directory that contains clang_rt.asan_dynamic-i386.lib (the import library)."""
    llvm_root = LLVM_PATH.parent
    for p in sorted(llvm_root.glob('lib/clang/*/lib/windows/clang_rt.asan_dynamic-i386.lib'), reverse=True):
        return p.parent  # take the newest version
    return None

def find_asan_runtime() -> typing.Optional[Path]:
    """Return the path of clang_rt.asan_dynamic-i386.dll from the LLVM installation, or None."""
    # Prefer bin/ (some installers drop a copy there for convenience)
    candidate = LLVM_PATH / ASAN_RUNTIME_DLL
    if candidate.exists():
        return candidate
    # Fall back to the versioned resource directory: lib/clang/*/lib/windows/
    lib_dir = find_asan_lib_dir()
    if lib_dir:
        candidate = lib_dir / ASAN_RUNTIME_DLL
        if candidate.exists():
            return candidate
    return None

def copy_asan_runtime(out_dir: Path, log: typing.IO):
    """Copy clang_rt.asan_dynamic-i386.dll into the output directory."""
    import shutil
    src = find_asan_runtime()
    if src is None:
        msg = (f'Warning: {ASAN_RUNTIME_DLL} not found under {LLVM_PATH.parent}.\n'
               f'The ASan-instrumented DLL will fail to load at runtime without it.\n'
               f'Copy the file manually from your LLVM installation to the output directory.\n')
        print(msg, end='')
        log.write(msg.encode())
        return
    dest = out_dir / ASAN_RUNTIME_DLL
    shutil.copy2(str(src), str(dest))
    msg = f'Copied ASan runtime: {src} -> {dest}\n'
    print(msg, end='')
    log.write(msg.encode())

def find_editbin() -> typing.Optional[str]:
    """Return the path to editbin.exe from a Visual Studio installation, or None."""
    import shutil, subprocess as sp
    # 1. Already on PATH (Developer Command Prompt)
    found = shutil.which('editbin.exe')
    if found:
        return found
    # 2. Locate VS via vswhere.exe (present since VS 2017)
    for vswhere in [
        r'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe',
        r'C:\Program Files\Microsoft Visual Studio\Installer\vswhere.exe',
    ]:
        if not os.path.exists(vswhere):
            continue
        try:
            r = sp.run([vswhere, '-latest', '-property', 'installationPath'],
                       capture_output=True, text=True, timeout=10)
            vs = r.stdout.strip()
            if not vs:
                continue
            # editbin.exe lives under VC/Tools/MSVC/<ver>/bin/Host*/x86/
            for pattern in ('VC/Tools/MSVC/*/bin/HostX86/x86/editbin.exe',
                            'VC/Tools/MSVC/*/bin/HostX64/x86/editbin.exe'):
                matches = sorted(Path(vs).glob(pattern), reverse=True)
                if matches:
                    return str(matches[0])
        except Exception:
            pass
    return None

def rebase_asan_runtime(out_dir: Path, log: typing.IO):
    """Rebase the copied clang_rt.asan_dynamic-i386.dll to ASAN_REBASE_ADDRESS.

    Without a fixed preferred base, ASLR can place the DLL anywhere in the
    32-bit address space.  If it lands inside the ASan shadow range
    [0x30000000-0x4fffffff], __asan_init() detects the overlap and aborts,
    making the game DLL fail to load with ERROR_DLL_INIT_FAILED (1114).
    """
    import subprocess as sp
    dll = out_dir / ASAN_RUNTIME_DLL
    if not dll.exists():
        return  # copy_asan_runtime already warned
    editbin = find_editbin()
    if editbin is None:
        msg = (f'Warning: editbin.exe not found; cannot rebase {ASAN_RUNTIME_DLL}.\n'
               f'ASLR may place the DLL inside the ASan shadow and abort on startup.\n'
               f'Fix manually:\n'
               f'  editbin /REBASE:BASE={ASAN_REBASE_ADDRESS} "{dll}"\n')
        print(msg, end='')
        log.write(msg.encode())
        return
    cmd = [editbin, f'/REBASE:BASE={ASAN_REBASE_ADDRESS}', str(dll)]
    msg = f'Rebasing ASan runtime to {ASAN_REBASE_ADDRESS}: {" ".join(cmd)}\n'
    print(msg, end='')
    log.write(msg.encode())
    cp = sp.run(cmd, capture_output=True)
    if cp.returncode != 0:
        err = cp.stderr.decode(errors='replace')
        msg = f'Warning: editbin /REBASE failed (exit {cp.returncode}):\n{err}\n'
    else:
        msg = f'Rebased {ASAN_RUNTIME_DLL} to {ASAN_REBASE_ADDRESS}\n'
    print(msg, end='')
    log.write(msg.encode())

def build_shadow_boot(cl: str, link: str, out_dir: Path, log: typing.IO):
    """Build asan_shadow_boot.dll — the shadow-range pre-reservation shim.

    This tiny DLL must be injected into CivilizationV.exe at process start via
    AppInit_DLLs so it runs before D3D/GPU driver initialisation can consume
    the ASan shadow range [0x2FFF0000-0x4FFFFFFF].

    The build uses clang-cl in two steps (compile then link) targeting x86
    with the dynamic CRT (/MD) and no extra dependencies beyond kernel32.
    """
    import subprocess as sp

    src = PROJECT_DIR / SHADOW_BOOT_SRC
    if not src.exists():
        msg = f'Warning: {src} not found; skipping {SHADOW_BOOT_DLL} build.\n'
        print(msg, end='')
        log.write(msg.encode())
        return

    obj  = out_dir / 'asan_shadow_boot.obj'
    dll  = out_dir / SHADOW_BOOT_DLL
    pdb  = out_dir / 'asan_shadow_boot.pdb'

    # --- Step 1: compile ---
    compile_cmd = [
        cl, '/nologo', '/W3', '/O2', '/MD', '/GS-',
        str(src), f'/Fo:{obj}', '/c',
    ]
    msg = f'Compiling {SHADOW_BOOT_SRC.name} ...\n'
    print(msg, end='')
    log.write(msg.encode())
    cp = sp.run(compile_cmd, capture_output=True)
    for chunk in (cp.stdout, cp.stderr):
        if chunk: log.write(chunk)
    if cp.returncode != 0:
        msg = f'Error: {SHADOW_BOOT_DLL} compile failed (exit {cp.returncode}).\n'
        print(msg, end='')
        log.write(msg.encode())
        return

    # --- Step 2: link ---
    link_cmd = [
        link, '/nologo', '/DLL', '/MACHINE:x86',
        str(obj), f'/OUT:{dll}', f'/PDB:{pdb}',
        '/SUBSYSTEM:WINDOWS',
        'kernel32.lib',
    ]
    msg = f'Linking {SHADOW_BOOT_DLL} ...\n'
    print(msg, end='')
    log.write(msg.encode())
    cp = sp.run(link_cmd, capture_output=True, cwd=str(out_dir))
    for chunk in (cp.stdout, cp.stderr):
        if chunk: log.write(chunk)
    if cp.returncode != 0:
        msg = f'Error: {SHADOW_BOOT_DLL} link failed (exit {cp.returncode}).\n'
        print(msg, end='')
        log.write(msg.encode())
        return

    msg = f'Built {SHADOW_BOOT_DLL} -> {dll}\n'
    print(msg, end='')
    log.write(msg.encode())

def build_asan_launcher(cl: str, link: str, out_dir: Path, log: typing.IO):
    """Build asan_launcher.exe — the preferred injection launcher.

    Creates CivilizationV.exe as a suspended process, injects asan_shadow_boot.dll
    via CreateRemoteThread, then resumes.  No registry changes, no elevation.

    Must be 32-bit (/MACHINE:x86): GetProcAddress("LoadLibraryW") must return
    the 32-bit kernel32 address that is valid inside the 32-bit game process.
    On 64-bit Windows all 32-bit processes share the same ASLR base for system
    DLLs (randomised once at boot), so the 32-bit address from our process is
    correct for the target.
    """
    import subprocess as sp

    src = PROJECT_DIR / ASAN_LAUNCHER_SRC
    if not src.exists():
        msg = f'Warning: {src} not found; skipping {ASAN_LAUNCHER_EXE} build.\n'
        print(msg, end='')
        log.write(msg.encode())
        return

    obj  = out_dir / 'asan_launcher.obj'
    exe  = out_dir / ASAN_LAUNCHER_EXE
    pdb  = out_dir / 'asan_launcher.pdb'

    # --- Step 1: compile ---
    # /MT (static CRT) makes the launcher self-contained — no MSVCR*.dll needed.
    # Matches Zenith-test build_zenith_launcher_v90.py which uses libcmt.lib.
    compile_cmd = [
        cl, '/nologo', '/W3', '/O2', '/MT', '/GS-',
        str(src), f'/Fo:{obj}', '/c',
    ]
    msg = f'Compiling {ASAN_LAUNCHER_SRC.name} ...\n'
    print(msg, end='')
    log.write(msg.encode())
    cp = sp.run(compile_cmd, capture_output=True)
    for chunk in (cp.stdout, cp.stderr):
        if chunk: log.write(chunk)
    if cp.returncode != 0:
        msg = f'Error: {ASAN_LAUNCHER_EXE} compile failed (exit {cp.returncode}).\n'
        print(msg, end='')
        log.write(msg.encode())
        return

    # --- Step 2: link as console EXE (static runtime, no CRT DLL dependency) ---
    link_cmd = [
        link, '/nologo', '/MACHINE:x86',
        str(obj), f'/OUT:{exe}', f'/PDB:{pdb}',
        '/SUBSYSTEM:CONSOLE',
        'kernel32.lib', 'advapi32.lib', 'libcmt.lib',
    ]
    msg = f'Linking {ASAN_LAUNCHER_EXE} ...\n'
    print(msg, end='')
    log.write(msg.encode())
    cp = sp.run(link_cmd, capture_output=True, cwd=str(out_dir))
    for chunk in (cp.stdout, cp.stderr):
        if chunk: log.write(chunk)
    if cp.returncode != 0:
        msg = f'Error: {ASAN_LAUNCHER_EXE} link failed (exit {cp.returncode}).\n'
        print(msg, end='')
        log.write(msg.encode())
        return

    msg = (
        f'Built {ASAN_LAUNCHER_EXE} -> {exe}\n'
        f'\n'
        f'  *** ASan session workflow ***\n'
        f'\n'
        f'  Copy both files to your Civ5 game directory (next to CivilizationV.exe):\n'
        f'    copy "{exe}" "<game_dir>"\n'
        f'    copy "{out_dir / SHADOW_BOOT_DLL}" "<game_dir>"\n'
        f'\n'
        f'  Then from that directory:\n'
        f'    set ASAN_OPTIONS=log_path=asan_game.log:halt_on_error=0:detect_leaks=0\n'
        f'    {ASAN_LAUNCHER_EXE} CivilizationV.exe\n'
        f'      -- or via env var --\n'
        f'    set CIVV_EXE=<full path to CivilizationV.exe>\n'
        f'    {ASAN_LAUNCHER_EXE}\n'
        f'\n'
        f'  In-game: main menu -> mod menu -> activate VP mod -> play.\n'
        f'  ASan reports: asan_game.log.<PID> next to the game EXE.\n'
    )
    print(msg, end='')
    log.write(msg.encode())

arg_parser = argparse.ArgumentParser(description='Build VP.')
arg_parser.add_argument('--config', type=str, default='debug', choices=['release', 'debug'])
arg_parser.add_argument(
    '--sanitizer', type=str, default='ubsan',
    choices=['none', 'ubsan', 'asan'],
    help='Sanitizer to enable for Debug builds (default: ubsan). '
         'Release builds always use none.')
args = arg_parser.parse_args()
config = Config.Release if args.config == 'release' else Config.Debug

# Sanitizer only applies to Debug; Release is always uninstrumented.
_san_map = {'none': Sanitizer.NONE, 'ubsan': Sanitizer.UBSAN, 'asan': Sanitizer.ASAN}
sanitizer = _san_map[args.sanitizer] if config == Config.Debug else Sanitizer.NONE

# Use 32-bit LLVM for sanitizer builds so that clang-rt is the right architecture.
if sanitizer in (Sanitizer.UBSAN, Sanitizer.ASAN):
    cl = str(LLVM_PATH / 'clang-cl.exe')
    link = str(LLVM_PATH / 'lld-link.exe')
else:
    cl = 'clang-cl.exe'
    link = 'lld-link.exe'
build_dir = PROJECT_DIR.joinpath(BUILD_DIR[config])
out_dir = PROJECT_DIR.joinpath(PROJECT_DIR, OUT_DIR[config])
cl_args = ' '.join(build_cl_config_args(config, sanitizer))
link_args = build_link_config_args(config, sanitizer)
pch_path = os.path.join(build_dir, PCH)
prepare_dirs(build_dir, out_dir)

log = open(out_dir.joinpath('build.log'), mode='w+b')
try:
    update_commit_id(log)
    build_clang_cpp(cl, cl_args, build_dir, log)
    build_pch(cl, cl_args, pch_path, build_dir, log)
    build_cpps(cl, cl_args, pch_path, build_dir, log)
    link_dll(link, link_args, build_dir, out_dir, log)
    if sanitizer == Sanitizer.ASAN:
        copy_asan_runtime(out_dir, log)
        rebase_asan_runtime(out_dir, log)
        build_shadow_boot(cl, link, out_dir, log)
        build_asan_launcher(cl, link, out_dir, log)
finally:
    log.close()
