#!/usr/bin/env python3
"""
Comprehensive case fixing script for Windows headers and libraries on Linux
Combines specific case mappings with automatic lowercase symlink creation
"""
import os
from pathlib import Path

def fix_header_case_issues():
    """Fix case sensitivity issues for Windows headers on Linux"""
    
    # Directories to fix
    dirs_to_fix = [
        Path('Dependencies/v7.0a_include'),
        Path('Dependencies/vc9_include')
    ]
    
    # Specific case mappings for commonly problematic headers
    # These are headers that we know cause issues and need specific mappings
    specific_mappings = {
        # Core Windows headers
        'windef.h': 'WinDef.h',
        'winnt.h': 'WinNT.h', 
        'winbase.h': 'WinBase.h',
        'winuser.h': 'WinUser.h',
        'wincon.h': 'WinCon.h',
        'winnls.h': 'WinNls.h',
        'winerror.h': 'WinError.h',
        'winreg.h': 'WinReg.h',
        'winsvc.h': 'WinSvc.h',
        'winsock.h': 'WinSock.h',
        'winsock2.h': 'WinSock2.h',
        'wininet.h': 'WinINet.h',
        'winioctl.h': 'WinIOCtl.h',
        'winperf.h': 'WinPerf.h',
        'wincrypt.h': 'WinCrypt.h',
        'wintrust.h': 'WinTrust.h',
        'winver.h': 'WinVer.h',
        'windowsx.h': 'WindowsX.h',
        
        # Driver and SAL headers
        'DriverSpecs.h': 'driverspecs.h',
        'specstrings.h': 'SpecStrings.h',
        'SpecStrings.h': 'specstrings.h',  # Reverse mapping too
        
        # Base type headers
        'basetsd.h': 'BaseTsd.h',
        'guiddef.h': 'Guiddef.h',
        
        # Pack headers
        'poppack.h': 'PopPack.h',
        'pshpack1.h': 'PshPack1.h',
        'pshpack2.h': 'PshPack2.h',
        'pshpack4.h': 'PshPack4.h',
        'pshpack8.h': 'PshPack8.h',
        
        # Other commonly problematic headers
        'tvout.h': 'Tvout.h',
        'sal.h': 'sal.h',  # Already correct case
    }
    
    for dir_path in dirs_to_fix:
        if not dir_path.exists():
            print(f"Directory {dir_path} does not exist, skipping")
            continue
            
        print(f"Fixing header case issues in {dir_path}")
        
        # First, get all header files in the directory
        header_files = {}
        for f in dir_path.glob('*.h'):
            header_files[f.name.lower()] = f.name
            
        # Step 1: Apply specific case mappings
        print(f"  Applying specific case mappings...")
        for lowercase, target in specific_mappings.items():
            target_lower = target.lower()
            
            # Check if the target file exists (case-insensitive)
            if target_lower in header_files:
                actual_file = header_files[target_lower]
                symlink_path = dir_path / lowercase
                
                # Only create symlink if it doesn't exist and names are different
                if actual_file != lowercase and not symlink_path.exists():
                    try:
                        symlink_path.symlink_to(actual_file)
                        print(f"    Created specific mapping: {lowercase} -> {actual_file}")
                    except OSError as e:
                        print(f"    Failed to create symlink {lowercase}: {e}")
        
        # Step 2: Create lowercase symlinks for all remaining headers
        print(f"  Creating lowercase symlinks for all headers...")
        for f in dir_path.glob('*.h'):
            lowercase_name = f.name.lower()
            if f.name != lowercase_name:
                symlink_path = dir_path / lowercase_name
                if not symlink_path.exists():
                    try:
                        symlink_path.symlink_to(f.name)
                        print(f"    Created lowercase symlink: {lowercase_name} -> {f.name}")
                    except OSError as e:
                        print(f"    Failed to create symlink {lowercase_name}: {e}")
        
        # Step 3: Create uppercase symlinks for headers that might be referenced in uppercase
        print(f"  Creating uppercase symlinks for common patterns...")
        uppercase_patterns = [
            'WINDOWS.H', 'WINDEF.H', 'WINNT.H', 'WINBASE.H', 'WINUSER.H'
        ]
        
        for pattern in uppercase_patterns:
            pattern_lower = pattern.lower()
            if pattern_lower in header_files:
                actual_file = header_files[pattern_lower]
                symlink_path = dir_path / pattern
                
                if actual_file != pattern and not symlink_path.exists():
                    try:
                        symlink_path.symlink_to(actual_file)
                        print(f"    Created uppercase symlink: {pattern} -> {actual_file}")
                    except OSError as e:
                        print(f"    Failed to create uppercase symlink {pattern}: {e}")

def fix_library_case_issues():
    """Fix case sensitivity issues for Windows libraries on Linux"""
    
    # Directories to fix
    lib_dirs_to_fix = [
        Path('Dependencies/v7.0a_lib'),
        Path('Dependencies/vc9_lib')
    ]
    
    # Specific library case mappings for commonly problematic libraries
    # These are libraries that pragma comments reference in lowercase
    specific_lib_mappings = {
        # Windows system libraries (pragma comments use lowercase)
        'winmm.lib': 'WinMM.Lib',
        'version.lib': 'Version.Lib', 
        'psapi.lib': 'Psapi.Lib',
        'dbghelp.lib': 'DbgHelp.Lib',
        'kernel32.lib': 'Kernel32.Lib',
        'user32.lib': 'User32.Lib',
        'gdi32.lib': 'Gdi32.Lib',
        'winspool.lib': 'WinSpool.Lib',
        'comdlg32.lib': 'ComDlg32.Lib',
        'advapi32.lib': 'AdvAPI32.Lib',
        'shell32.lib': 'shell32.lib',  # Already lowercase
        'ole32.lib': 'Ole32.Lib',
        'oleaut32.lib': 'OleAut32.Lib',
        'uuid.lib': 'Uuid.Lib',
        'odbc32.lib': 'odbc32.lib',    # Already lowercase
        'odbccp32.lib': 'odbccp32.lib', # Already lowercase
        
        # VC runtime libraries (pragma comments use lowercase)
        'msvcrt.lib': 'msvcrt.lib',    # Already lowercase in vc9_lib
        'MSVCRT.lib': 'msvcrt.lib',    # Build script may reference uppercase version
        'libcpmt.lib': 'libcpmt.lib',  # Already lowercase in vc9_lib
        'comsuppw.lib': 'comsuppw.lib', # Already lowercase in vc9_lib
        'msvcprt.lib': 'msvcprt.lib',  # Already lowercase in vc9_lib
        'oldnames.lib': 'oldnames.lib', # Already lowercase in vc9_lib
        'OLDNAMES.lib': 'oldnames.lib', # Build script references uppercase version
    }
    
    for lib_dir in lib_dirs_to_fix:
        if not lib_dir.exists():
            print(f"Library directory {lib_dir} does not exist, skipping")
            continue
            
        print(f"Fixing library case issues in {lib_dir}")
        
        # Get all library files in the directory
        lib_files = {}
        for f in list(lib_dir.glob('*.lib')) + list(lib_dir.glob('*.Lib')):
            lib_files[f.name.lower()] = f.name
            
        # Apply specific library case mappings
        print(f"  Applying specific library case mappings...")
        for lowercase, target in specific_lib_mappings.items():
            target_lower = target.lower()
            
            # Check if the target file exists (case-insensitive)
            if target_lower in lib_files:
                actual_file = lib_files[target_lower]
                symlink_path = lib_dir / lowercase
                
                # Only create symlink if it doesn't exist and names are different
                if actual_file != lowercase and not symlink_path.exists():
                    try:
                        symlink_path.symlink_to(actual_file)
                        print(f"    Created library mapping: {lowercase} -> {actual_file}")
                    except OSError as e:
                        print(f"    Failed to create library symlink {lowercase}: {e}")
        
        # Create lowercase symlinks for all remaining libraries
        print(f"  Creating lowercase symlinks for all libraries...")
        for f in list(lib_dir.glob('*.lib')) + list(lib_dir.glob('*.Lib')):
            lowercase_name = f.name.lower()
            if f.name != lowercase_name:
                symlink_path = lib_dir / lowercase_name
                if not symlink_path.exists():
                    try:
                        symlink_path.symlink_to(f.name)
                        print(f"    Created lowercase library symlink: {lowercase_name} -> {f.name}")
                    except OSError as e:
                        print(f"    Failed to create library symlink {lowercase_name}: {e}")
        
        # Create mixed-case symlinks (e.g., Version.lib for Version.Lib)
        print(f"  Creating mixed-case symlinks for libraries...")
        for f in list(lib_dir.glob('*.Lib')):
            # Create .lib version of .Lib files
            mixed_case_name = f.name[:-4] + '.lib'  # Replace .Lib with .lib
            if f.name != mixed_case_name:
                symlink_path = lib_dir / mixed_case_name
                if not symlink_path.exists():
                    try:
                        symlink_path.symlink_to(f.name)
                        print(f"    Created mixed-case library symlink: {mixed_case_name} -> {f.name}")
                    except OSError as e:
                        print(f"    Failed to create mixed-case library symlink {mixed_case_name}: {e}")
        
        # Create uppercase symlinks for libraries that might be referenced in uppercase
        print(f"  Creating uppercase symlinks for libraries...")
        for f in list(lib_dir.glob('*.lib')):
            # Create uppercase version of lowercase .lib files
            uppercase_name = f.name.upper()
            if f.name != uppercase_name:
                symlink_path = lib_dir / uppercase_name
                if not symlink_path.exists():
                    try:
                        symlink_path.symlink_to(f.name)
                        print(f"    Created uppercase library symlink: {uppercase_name} -> {f.name}")
                    except OSError as e:
                        print(f"    Failed to create uppercase library symlink {uppercase_name}: {e}")
        
        # Create symlinks without .lib extension for DEFAULTLIB directives
        print(f"  Creating symlinks without .lib extension for DEFAULTLIB directives...")
        critical_libs = ['msvcrt.lib', 'oldnames.lib', 'msvcprt.lib', 'libcmt.lib', 'libcpmt.lib']
        for lib_file in critical_libs:
            if (lib_dir / lib_file).exists():
                # Create symlink without .lib extension (e.g., MSVCRT -> msvcrt.lib)
                base_name = lib_file[:-4]  # Remove .lib extension
                for case_variant in [base_name.upper(), base_name.lower(), base_name]:
                    symlink_path = lib_dir / case_variant
                    if not symlink_path.exists() and case_variant != lib_file:
                        try:
                            symlink_path.symlink_to(lib_file)
                            print(f"    Created DEFAULTLIB symlink: {case_variant} -> {lib_file}")
                        except OSError as e:
                            print(f"    Failed to create DEFAULTLIB symlink {case_variant}: {e}")

def main():
    """Main function"""
    print("Starting comprehensive case fixing...")
    fix_header_case_issues()
    fix_library_case_issues()
    print("Case fixing completed!")

if __name__ == '__main__':
    main()