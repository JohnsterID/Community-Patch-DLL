import os
import re
import subprocess
from enum import Enum
import typing
import time
import tempfile
from pathlib import Path
import argparse
from queue import Queue
import json
import sys

def ensure_headers_fixed():
    """Ensure fix_header_case_issues.py has been run (auto-run if needed)"""
    # Check if case-insensitive symlinks exist (sentinel check)
    sentinel = Path('Dependencies/v7.0a_include/windef.h')
    
    if not sentinel.exists():
        print("=" * 70)
        print("FIRST-TIME SETUP: Creating case-insensitive header symlinks...")
        print("=" * 70)
        print("\nLinux uses case-sensitive filesystems, but Windows headers use")
        print("mixed case. Creating symlinks for case-insensitive includes...\n")
        
        try:
            # Import and run the fix script
            import fix_header_case_issues
            fix_header_case_issues.main()
            print("\nSUCCESS: Setup complete! Continuing with build...\n")
        except Exception as e:
            print(f"\nFAILED: ERROR: Failed to run fix_header_case_issues.py: {e}")
            print("\nTry running manually:")
            print("    python3 fix_header_case_issues.py\n")
            sys.exit(1)
    # If sentinel exists, headers are already fixed - continue silently

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
    'enum-constexpr-conversion',      # #9786 - defaults to error in clang 16+ (GET_PLAYER(BARBARIAN_PLAYER) in CvUnit.cpp); unknown -Wno- flags are harmless on clang 14
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
def cpp_files_from_vcxproj():
    """Derive the compile list from VoxPopuli.vcxproj so the script stays in
    sync with the project on any branch (avoids manual list maintenance when
    files like SqliteLogger.cpp are added upstream)."""
    vcxproj = Path('CvGameCoreDLL_Expansion2/VoxPopuli.vcxproj')
    if not vcxproj.exists():
        print(f'ERROR: {vcxproj} not found (run from the repository root).')
        sys.exit(1)
    files = re.findall(r'<ClCompile Include="([^"]+)"', vcxproj.read_text())
    cpps = []
    for f in files:
        f = f.replace('\\', '/')
        if f == '_precompile.cpp':
            continue  # built separately as the PCH object
        cpps.append(f'CvGameCoreDLL_Expansion2/{f}')
    if not cpps:
        print(f'ERROR: no <ClCompile> entries parsed from {vcxproj}.')
        sys.exit(1)
    missing = [c for c in cpps if not Path(c).exists()]
    if missing:
        print('ERROR: files listed in VoxPopuli.vcxproj are missing on disk:')
        for c in missing:
            print(f'  {c}')
        sys.exit(1)
    return cpps

CPP = cpp_files_from_vcxproj()

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

def build_cpps(cl: str, cl_args: list[str], pch_path: Path, build_dir: Path, log: typing.IO, analyze: bool = False, export_compile_commands: bool = False):
    if analyze:
        print('running static analysis on cpps...')
    else:
        print('building cpps...')
    start_time = time.time()
    build_tasks = TaskMan()
    logs: dict[Path, typing.IO] = {}
    compile_commands = []
    try:
        for cpp in CPP:
            cpp_src = PROJECT_DIR.joinpath(cpp)
            cpp_log = tempfile.TemporaryFile()
            logs[cpp_src] = cpp_log
            
            if analyze:
                # Run static analysis instead of compilation
                # Use --analyzer-output plist-multi-file with LLVM 21.1.8
                out = build_dir.joinpath(cpp).with_suffix('.plist')
                command = [cl, '--analyze', '--analyzer-output', 'plist-multi-file', str(cpp_src), '-o', str(out)] + cl_args
            else:
                # Regular compilation
                out = build_dir.joinpath(cpp).with_suffix('.obj')
                command = [cl, str(cpp_src), '-o', str(out)] + cl_args
                
                # Add -MJ flag for compile commands export if requested
                if export_compile_commands:
                    compile_db_entry = build_dir.joinpath(f'{Path(cpp).stem}.json')
                    command.extend(['-MJ', str(compile_db_entry)])
            
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
            if analyze:
                print(f'{failed} cpp(s) had analysis issues - see build log')
            else:
                print(f'{failed} cpp(s) failed to build - see build log')
                quit()
        end_time = time.time()
        if analyze:
            print(f'static analysis finished after {end_time - start_time} seconds')
        else:
            print(f'cpps build finished after {end_time - start_time} seconds')
            
        # Combine individual JSON files into compile_commands.json if requested
        if export_compile_commands and not analyze:
            combine_compile_commands(build_dir)
            
    finally:
       del logs

def combine_compile_commands(build_dir: Path):
    """Combine individual -MJ JSON files into a single compile_commands.json"""
    print('generating compile_commands.json...')
    compile_commands = []
    
    # Find all .json files generated by -MJ
    json_files = list(build_dir.glob('*.json'))
    
    for json_file in json_files:
        try:
            with open(json_file, 'r') as f:
                content = f.read().strip()
                # -MJ generates JSON objects with trailing commas, one per line
                # Split by lines and parse each JSON object
                for line in content.split('\n'):
                    line = line.strip()
                    if line:
                        # Remove trailing comma if present
                        if line.endswith(','):
                            line = line[:-1]
                        try:
                            entry = json.loads(line)
                            compile_commands.append(entry)
                        except json.JSONDecodeError as e:
                            print(f'Warning: Could not parse line in {json_file}: {line[:100]}... Error: {e}')
        except FileNotFoundError as e:
            print(f'Warning: Could not read {json_file}: {e}')
    
    # Write combined compile_commands.json to project root
    output_file = PROJECT_DIR.joinpath('compile_commands.json')
    with open(output_file, 'w') as f:
        json.dump(compile_commands, f, indent=2)
    
    print(f'Generated {output_file} with {len(compile_commands)} entries')
    
    # Clean up individual JSON files
    for json_file in json_files:
        json_file.unlink()

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
arg_parser.add_argument('--analyze', action='store_true', help='Run static analysis with clang --analyze')
arg_parser.add_argument('--export-compile-commands', action='store_true', help='Generate compile_commands.json for clang-tidy')
args = arg_parser.parse_args()
config = Config.Release if args.config == 'release' else Config.Debug

def resolve_toolchain():
    """Locate clang and lld-link.

    Order: $LLVM_PATH/bin if set, else whatever is on PATH (works with distro
    packages, e.g. apt clang+lld). Fails with actionable instructions instead
    of an obscure exec error mid-build."""
    import shutil
    llvm_path = os.environ.get('LLVM_PATH')
    if llvm_path:
        cl = os.path.join(llvm_path, 'bin', 'clang')
        link = os.path.join(llvm_path, 'bin', 'lld-link')
        if not os.path.isfile(cl):
            print(f'ERROR: LLVM_PATH is set but {cl} does not exist.')
            sys.exit(1)
        if not os.path.isfile(link):
            link = 'lld-link'  # fall back to PATH for the linker only
    else:
        cl = shutil.which('clang')
        link = shutil.which('lld-link')
        if not cl or not link:
            print('ERROR: clang and/or lld-link not found.')
            print('Install them (e.g. apt-get install clang lld) or set LLVM_PATH')
            print('to an LLVM distribution root containing bin/clang and bin/lld-link.')
            sys.exit(1)
    try:
        ver = subprocess.run([cl, '--version'], capture_output=True, text=True).stdout.splitlines()[0]
        print(f'Toolchain: {ver} ({cl})')
    except OSError as e:
        print(f'ERROR: cannot execute {cl}: {e}')
        sys.exit(1)
    return cl, link

def check_dependencies():
    """Verify the VC9/WinSDK cross-compile headers and libs are present, with
    bootstrap instructions if not."""
    missing = [p for p in INCLUDE_PATHS + LIB_PATHS if not os.path.isdir(p)]
    if missing:
        print('ERROR: cross-compile dependencies are missing:')
        for p in missing:
            print(f'  {p}')
        print('''
Provide the VC9 SP1 + Windows SDK 7.0 headers/libs under Dependencies/:
  Dependencies/v7.0a_include  (WinSDK Include)
  Dependencies/v7.0a_lib      (WinSDK Lib)
  Dependencies/vc9_include    (VC9 include)
  Dependencies/vc9_lib        (VC9 lib)

Either extract v90-dependencies.zip here, or bootstrap from the official
SDK 7.0 ISO with extract-vc9.sh from https://github.com/JohnsterID/vc9-toolset
(needs p7zip-full + msitools), then symlink/move its output:
  VC/include -> vc9_include,  VC/lib -> vc9_lib
  WinSDK/Include -> v7.0a_include,  WinSDK/Lib -> v7.0a_lib
Directory symlinks are fine; fix_header_case_issues.py follows them.''')
        sys.exit(1)

# Order matters: verify Dependencies/ exists before the header case-fixer
# tries to walk it, so a missing-deps setup fails with instructions.
check_dependencies()
ensure_headers_fixed()
cl, link = resolve_toolchain()
build_dir = PROJECT_DIR.joinpath(BUILD_DIR[config])
out_dir = PROJECT_DIR.joinpath(PROJECT_DIR, OUT_DIR[config])
cl_args = build_cl_config_args(config)
link_args = build_link_config_args(config)
pch_path = os.path.join(build_dir, PCH)
prepare_dirs(build_dir, out_dir)

log = open(out_dir.joinpath('build.log'), mode='w+b')
try:
    update_commit_id(log)
    if not args.analyze:
        build_clang_cpp(cl, cl_args, build_dir, log)
        build_threading_stub(cl, build_dir, log)
        build_pch(cl, cl_args, pch_path, build_dir, log)
    
    # Run compilation or analysis
    build_cpps(cl, cl_args, pch_path, build_dir, log, analyze=args.analyze, export_compile_commands=args.export_compile_commands)
    
    if not args.analyze:
        link_dll(link, link_args, build_dir, out_dir, log)
    else:
        print('Static analysis completed. Check .plist files in build directory for results.')
finally:
    log.close()