#!/usr/bin/env python3
"""
Automated Clang-Tidy Script for VS2008/C++03 Compatibility with CRLF Workaround

IMPORTANT: cppcoreguidelines-init-variables RULES
==================================================

This check tries to initialize ALL uninitialized variables, but VS2008/C++03 has special cases:

1. va_list variables - CANNOT be initialized in VS2008!
   ❌ va_list args = NULL;  // Compilation error C2552
   ✅ va_list args;          // Correct - use va_start() to initialize
   
   Why: va_list is an aggregate type in VS2008 that requires va_start() for initialization.
        Any attempt to initialize it directly causes C2552 error.

2. Pointers get nullptr → NULL conversion
   🔄 int* ptr = nullptr;  →  int* ptr = NULL;  // Auto-converted by this script

3. Function parameters - Skip initialization
   Already initialized by caller, don't add "= 0"

4. Variables immediately assigned - May skip
   int x;
   x = getValue();  // Initialization would be redundant

5. Loop counters - May skip 
   for (int i = 0; ...)  // Already initialized in loop

6. Static variables - Usually skip
   Have default zero-initialization already

7. const variables - Must initialize anyway
   const int x = 5;  // Already required by C++

8. Arrays - Special initialization syntax
   int arr[10] = {0};  // OK in VS2008

9. Structs/classes - May need {}
   struct Point p = {0, 0};  // OK in C++03
   NOT: Point p = {};        // C++11 only

10. Member variables - Use initializer lists
    Class() : member(0) {}   // Preferred in C++03
    NOT: int member = 0;     // C++11 inline member initialization

FILTERING APPLIED BY THIS SCRIPT:
- Filters va_list initialization attempts (compilation error)
- Converts nullptr to NULL (C++03 compatibility)
- Filters NAN usage (not standard in VS2008)
- Filters std::to_string (not available in VS2008)
- Filters va_arg corruption patterns
- Detects and filters corruption patterns

CRLF WORKAROUND:
- Converts CRLF → LF before running clang-tidy (offset compatibility)
- Runs analysis with proper byte offsets
- Converts LF → CRLF after applying fixes (restores original format)
"""

import subprocess
import sys
import time
import re
import shutil
from pathlib import Path
from collections import defaultdict

try:
    import yaml
except ImportError:
    print("Error: PyYAML is required but not installed")
    print("Install it with: pip3 install pyyaml")
    print("Or: pip3 install -r requirements.txt")
    sys.exit(1)

# LLVM tools paths
CLANG_TIDY = "/tmp/LLVM-21.1.8-Linux-X64/bin/clang-tidy"
CLANG_APPLY_REPLACEMENTS = "/tmp/LLVM-21.1.8-Linux-X64/bin/clang-apply-replacements"

# Proven checks from clang-tidy_notes.txt
PROVEN_CHECKS = [
    "readability-isolate-declaration",  # REQUIRED FIRST
    "cppcoreguidelines-init-variables",  # WITH VS2008 CONVERSION
    "readability-inconsistent-declaration-parameter-name",
    "modernize-use-bool-literals",
    "readability-simplify-boolean-expr",  # REQUIRES MANUAL REVIEW
    "readability-container-size-empty",
    "readability-string-compare",
    "readability-avoid-return-with-void-value",
    "readability-redundant-declaration",
    "readability-redundant-function-ptr-dereference",
    "readability-redundant-smartptr-get",
    "readability-redundant-string-cstr",
    "readability-redundant-string-init",
    "readability-static-accessed-through-instance"
]

def check_prerequisites():
    """Check if all required tools are available"""
    print("Checking prerequisites...")
    
    # Check clang-tidy
    if not Path(CLANG_TIDY).exists():
        print(f"❌ clang-tidy not found at {CLANG_TIDY}")
        return False
    
    # Check clang-apply-replacements
    if not Path(CLANG_APPLY_REPLACEMENTS).exists():
        print(f"❌ clang-apply-replacements not found at {CLANG_APPLY_REPLACEMENTS}")
        return False
    
    # Check compile_commands.json
    if not Path("compile_commands.json").exists():
        print("❌ compile_commands.json not found")
        return False
    
    print("✓ All prerequisites satisfied")
    return True

def find_source_files():
    """Find all C++ source files"""
    cpp_files = list(Path("CvGameCoreDLL_Expansion2").glob("*.cpp"))
    print(f"Found {len(cpp_files)} C++ source files")
    return cpp_files

def convert_cpp11_to_cpp03(replacement_text, context=""):
    """Convert C++11 constructs to VS2008/C++03 compatible equivalents"""
    if not replacement_text:
        return replacement_text
    
    # Convert nullptr to appropriate C++03 equivalent
    if "= nullptr" in replacement_text:
        # For pointers, use = NULL
        return replacement_text.replace("= nullptr", " = NULL")
    
    # Convert other C++11 constructs as needed
    # Add more conversions here as discovered
    
    return replacement_text

def is_problematic_for_vs2008(replacement_text, file_path="", context=""):
    """Check if a replacement is problematic for VS2008/C++03"""
    if not replacement_text:
        return False
    
    # Check replacement text for problematic patterns
    problematic_patterns = [
        r'#include <math\.h>',  # math.h additions can cause issues
        r'= NAN',  # NAN is not standard in VS2008
        r'std::to_string',  # std::to_string not available in VS2008
        r'va_arg\([^,]+\s*=\s*[^,]+,',  # va_arg with assignment inside - VS2008 incompatible
        r'\)\);$',  # Double closing parentheses - syntax error
        # CORRUPTION DETECTION - clang-tidy bugs that insert "= NULL" in wrong places
        r'= NULL[a-z]',  # Detect NULL insertion corruption like "strle = NULLn"
        r'[a-z]= NULL[a-z]',  # Detect NULL insertion in middle of words
        r'\w+\(\)\);',  # Detect extra closing parentheses in function calls
        r'strle\s*=',  # Detect strlen corruption specifically
        r'p\s*=\s*NULL\w+',  # Detect "p = NULLrocessing" type corruptions
        r'argum\s*=\s*NULL',  # Detect "argum = NULLent" type corruptions
        r'va\s*=\s*NULL\w+',  # Detect "va = NULLriable" type corruptions
        r'va_arg\([^,]+\s*=\s*NULL',  # Detect va_arg(vl = NULL, ...) corruption
        r'\w+\(\)\);$',  # Detect function calls with extra closing parenthesis
        # Add specific problematic initializations from notes
        r'Connections\s*=\s*0',
        r'VoteCommitmentList\s+\w+\s*=\s*0',
        r'IDInfoVector\s+\w+\s*=\s*0',
        r'LookupTable\s+\w+\s*=\s*0',
        r'TurnData\s+\w+\s*=\s*0',
        r'PlotStatePerTurn\s+\w+\s*=\s*0',
    ]
    
    for pattern in problematic_patterns:
        if re.search(pattern, replacement_text):
            return True
    
    # Check context for va_list initialization - VS2008 incompatible
    if context and re.search(r'=\s*(NULL|nullptr|\{\})$', replacement_text.strip()):
        if re.search(r'va_list\s+\w+', context):
            return True
    
    # Additional check for va_arg corruption
    if context and 'va_arg' in context:
        if re.search(r'va_arg\([^,]+\s*=', replacement_text):
            return True
    
    # Check for function name corruptions in context
    if context:
        # Look for patterns where function names might be corrupted
        if re.search(r'strlen\s*\(', context) and 'strle' in replacement_text:
            return True
        if re.search(r'processing', context) and 'p = NULL' in replacement_text:
            return True
        if re.search(r'argument', context) and 'argum = NULL' in replacement_text:
            return True
        if re.search(r'variable', context) and 'va = NULL' in replacement_text:
            return True
    
    # Check for suspicious patterns that suggest corruption
    if re.search(r'\w+\s*=\s*NULL\w+', replacement_text):
        return True
    
    return False

def merge_overlapping_replacements(replacements):
    """Merge or resolve overlapping replacements to prevent corruption"""
    if not replacements:
        return replacements
    
    # Sort by offset
    sorted_replacements = sorted(replacements, key=lambda r: r.get('Offset', 0))
    
    merged = []
    for replacement in sorted_replacements:
        offset = replacement.get('Offset', 0)
        length = replacement.get('Length', 0)
        
        # Check for overlap with previous replacement
        if merged:
            prev = merged[-1]
            prev_end = prev.get('Offset', 0) + prev.get('Length', 0)
            
            # If overlapping, skip this replacement (keep first one)
            if offset < prev_end:
                print(f"  Skipping overlapping replacement at offset {offset}")
                continue
        
        merged.append(replacement)
    
    return merged

def process_and_filter_fixes(fixes_file):
    """Process fixes with VS2008 compatibility and overlap resolution"""
    if not fixes_file.exists():
        print(f"Fixes file not found: {fixes_file}")
        return None
    
    print("Processing and filtering fixes for VS2008/C++03 compatibility...")
    
    try:
        with open(fixes_file, 'r') as f:
            data = yaml.safe_load(f)
        
        if not data:
            print("No data in fixes file")
            return None
        
        # Group replacements by file to handle overlaps
        files_replacements = defaultdict(list)
        
        # Process each diagnostic
        if isinstance(data, list):
            # Handle list of diagnostics
            diagnostics = data
        elif isinstance(data, dict) and 'Diagnostics' in data:
            # Handle single file format
            diagnostics = data['Diagnostics']
        else:
            # Handle direct diagnostic format
            diagnostics = [data] if 'DiagnosticMessage' in data else []
        
        processed_diagnostics = []
        filtered_count = 0
        converted_count = 0
        
        for diagnostic in diagnostics:
            if 'DiagnosticMessage' not in diagnostic:
                processed_diagnostics.append(diagnostic)
                continue
            
            message = diagnostic['DiagnosticMessage']
            if 'Replacements' not in message:
                processed_diagnostics.append(diagnostic)
                continue
            
            replacements = message['Replacements']
            if not replacements:
                processed_diagnostics.append(diagnostic)
                continue
            
            # Process replacements for this diagnostic
            processed_replacements = []
            
            for replacement in replacements:
                file_path = replacement.get('FilePath', '')
                replacement_text = replacement.get('ReplacementText', '')
                offset = replacement.get('Offset', 0)
                
                # Read context around replacement for better decisions
                context = ""
                try:
                    if file_path and Path(file_path).exists():
                        with open(file_path, 'r') as f:
                            content = f.read()
                            start = max(0, offset - 100)
                            end = min(len(content), offset + 100)
                            context = content[start:end]
                except:
                    pass
                
                # Check if problematic for VS2008
                if is_problematic_for_vs2008(replacement_text, file_path, context):
                    print(f"  Filtered problematic: '{replacement_text.strip()}' in {Path(file_path).name}")
                    filtered_count += 1
                    continue
                
                # Convert C++11 to C++03
                original_text = replacement_text
                converted_text = convert_cpp11_to_cpp03(replacement_text, context)
                
                if converted_text != original_text:
                    print(f"  Converted: '{original_text.strip()}' → '{converted_text.strip()}' in {Path(file_path).name}")
                    replacement['ReplacementText'] = converted_text
                    converted_count += 1
                
                processed_replacements.append(replacement)
            
            # Group by file for overlap resolution
            for replacement in processed_replacements:
                file_path = replacement.get('FilePath', '')
                files_replacements[file_path].append(replacement)
            
            # Update diagnostic with processed replacements
            message['Replacements'] = processed_replacements
            processed_diagnostics.append(diagnostic)
        
        # Resolve overlaps within each file
        overlap_resolved_count = 0
        for file_path, replacements in files_replacements.items():
            original_count = len(replacements)
            merged = merge_overlapping_replacements(replacements)
            if len(merged) < original_count:
                overlap_resolved_count += original_count - len(merged)
                print(f"  Resolved {original_count - len(merged)} overlapping replacements in {Path(file_path).name}")
        
        # Update data structure
        if isinstance(data, list):
            result_data = processed_diagnostics
        elif isinstance(data, dict) and 'Diagnostics' in data:
            data['Diagnostics'] = processed_diagnostics
            result_data = data
        else:
            result_data = processed_diagnostics[0] if processed_diagnostics else {}
        
        # Write processed fixes
        processed_file = fixes_file.with_suffix('.processed.yaml')
        with open(processed_file, 'w') as f:
            yaml.dump(result_data, f, default_flow_style=False)
        
        print(f"Filtered {filtered_count} problematic fixes")
        print(f"Converted {converted_count} C++11 to C++03 fixes")
        print(f"Resolved {overlap_resolved_count} overlapping replacements")
        print(f"Processed fixes saved to: {processed_file}")
        
        return processed_file
        
    except Exception as e:
        print(f"Error processing fixes: {e}")
        import traceback
        traceback.print_exc()
        return None

def validate_applied_fixes():
    """Validate that applied fixes don't contain corruption patterns"""
    corruption_patterns = [
        r'va_arg\([^,]+\s*=\s*NULL',  # va_arg(vl = NULL, ...)
        r'= NULL[a-z]',  # strle = NULLn
        r'[a-z]= NULL[a-z]',  # p = NULLrocessing
        r'\w+\(\)\);',  # function()) - extra parenthesis
    ]
    
    source_files = find_source_files()
    corruption_found = False
    
    for file_path in source_files:
        try:
            with open(file_path, 'r') as f:
                content = f.read()
                
            for pattern in corruption_patterns:
                matches = re.findall(pattern, content)
                if matches:
                    print(f"❌ Corruption found in {file_path}: {matches}")
                    corruption_found = True
        except Exception as e:
            print(f"Warning: Could not validate {file_path}: {e}")
    
    return corruption_found

def run_combined_analysis():
    """Run all proven checks in a single clang-tidy invocation"""
    print("\n" + "="*60)
    print("Running All Proven Checks Combined (VS2008 Compatible)")
    print("="*60)
    
    source_files = find_source_files()
    if not source_files:
        print("No source files found")
        return False
    
    checks = ",".join(PROVEN_CHECKS)
    results_file = "clang-tidy-combined-results.txt"
    fixes_file = Path("clang-tidy-combined-fixes.yaml")
    
    print(f"Checks: {checks}")
    print(f"Source files: {len(source_files)}")
    print(f"Results: {results_file}")
    print(f"Fixes: {fixes_file}")
    
    # Build command
    cmd = [
        CLANG_TIDY,
        f"--checks={checks}",
        "--export-fixes=" + str(fixes_file),
        "--format-style=file",
        "-p", ".",
    ] + [str(f) for f in source_files]
    
    start_time = time.time()
    
    try:
        with open(results_file, 'w') as f:
            result = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, text=True)
        
        duration = time.time() - start_time
        print(f"clang-tidy completed in {duration:.1f} seconds ({duration/60:.1f} minutes) (exit code: {result.returncode})")
        
        # TDD: Check file size after clang-tidy runs
        test_file = Path("CvGameCoreDLL_Expansion2/CvGame.cpp")
        if test_file.exists():
            size_after_tidy = len(test_file.read_bytes())
            print(f"🔍 TDD: CvGame.cpp size after clang-tidy: {size_after_tidy} bytes")
        
        if fixes_file.exists():
            # Process and filter fixes
            processed_file = process_and_filter_fixes(fixes_file)
            
            if processed_file:
                print(f"Applying processed fixes from {processed_file}")
                
                # TDD: Check file size before applicator runs
                test_file = Path("CvGameCoreDLL_Expansion2/CvGame.cpp")
                if test_file.exists():
                    size_before_apply = len(test_file.read_bytes())
                    print(f"🔍 TDD: CvGame.cpp size before applicator: {size_before_apply} bytes")
                
                # Use custom YAML applicator instead of buggy clang-apply-replacements
                try:
                    from apply_yaml_fixes import YAMLFixApplicator
                    
                    applicator = YAMLFixApplicator(processed_file, dry_run=False, verbose=False)
                    
                    if not applicator.load_yaml():
                        print("❌ Failed to load YAML")
                        return False
                    
                    if not applicator.apply_all():
                        print("❌ Failed to apply fixes")
                        return False
                    
                    print("✓ Custom applicator successfully applied fixes")
                    
                    # Validate applied fixes for corruption
                    corruption_found = validate_applied_fixes()
                    if corruption_found:
                        print("❌ Corruption detected in applied fixes!")
                        return False
                    
                except ImportError as e:
                    print(f"❌ Could not import apply_yaml_fixes: {e}")
                    print("Make sure apply_yaml_fixes.py is in the same directory")
                    return False
                except Exception as e:
                    print(f"❌ Error applying fixes: {e}")
                    return False
            else:
                print("❌ Failed to process fixes")
                return False
        else:
            print("No fixes file generated")
        
        print("\n" + "="*60)
        print("SUMMARY")
        print("="*60)
        print(f"Total time: {duration:.1f} seconds ({duration/60:.1f} minutes)")
        print(f"Checks processed: {len(PROVEN_CHECKS)}")
        print(f"Results saved in: {results_file}")
        print(f"Processed fixes applied with VS2008/C++03 compatibility")
        
        return True
        
    except Exception as e:
        print(f"Error running clang-tidy: {e}")
        return False

def convert_files_to_lf():
    """Convert all source files from CRLF to LF for clang-tidy compatibility"""
    print("=" * 80)
    print("STEP 1: Converting CRLF → LF (required for clang-tidy offset compatibility)")
    print("=" * 80)
    
    source_dir = Path("CvGameCoreDLL_Expansion2")
    crlf_files = []
    
    for cpp_file in source_dir.glob("*.cpp"):
        try:
            content = cpp_file.read_bytes()
            if b'\r\n' in content:
                crlf_files.append(str(cpp_file))
                # Convert CRLF to LF
                content_lf = content.decode('utf-8').replace('\r\n', '\n').encode('utf-8')
                cpp_file.write_bytes(content_lf)
        except Exception as e:
            print(f"Warning: Could not process {cpp_file}: {e}")
    
    print(f"✓ Converted {len(crlf_files)} files from CRLF to LF")
    print(f"  (CRLF causes byte offset mismatches in clang-tidy)\n")
    
    return crlf_files

def convert_files_to_crlf(crlf_files):
    """Convert source files back from LF to CRLF"""
    print("\n" + "=" * 80)
    print("FINAL STEP: Converting LF → CRLF (restoring original line endings)")
    print("=" * 80)
    
    for file_path in crlf_files:
        try:
            cpp_file = Path(file_path)
            if cpp_file.exists():
                content = cpp_file.read_bytes()
                # Convert LF to CRLF
                content_crlf = content.decode('utf-8').replace('\n', '\r\n').encode('utf-8')
                cpp_file.write_bytes(content_crlf)
        except Exception as e:
            print(f"Warning: Could not restore {file_path}: {e}")
    
    print(f"✓ Restored CRLF line endings in {len(crlf_files)} files\n")

def backup_files(file_list):
    """Create .bak copies before modifying"""
    print("Creating backup files...")
    backed_up = 0
    for file_path in file_list:
        try:
            backup_path = Path(f"{file_path}.bak")
            shutil.copy2(file_path, backup_path)
            backed_up += 1
        except Exception as e:
            print(f"Warning: Could not backup {file_path}: {e}")
    print(f"✓ Created {backed_up} backup files\n")

def restore_from_backup(file_list):
    """Restore from .bak files"""
    print("\n" + "=" * 80)
    print("RESTORING FILES FROM BACKUP")
    print("=" * 80)
    
    restored = 0
    for file_path in file_list:
        try:
            backup_path = Path(f"{file_path}.bak")
            if backup_path.exists():
                shutil.copy2(backup_path, file_path)
                backup_path.unlink()
                restored += 1
        except Exception as e:
            print(f"Warning: Could not restore {file_path}: {e}")
    
    print(f"✓ Restored {restored} files from backup\n")

def cleanup_backups(file_list):
    """Remove backup files after successful completion"""
    for file_path in file_list:
        backup_path = Path(f"{file_path}.bak")
        if backup_path.exists():
            try:
                backup_path.unlink()
            except:
                pass

def rollback_to_git(file_paths):
    """Restore files to git HEAD state"""
    print("\n" + "=" * 80)
    print("ROLLING BACK CHANGES VIA GIT")
    print("=" * 80)
    
    try:
        # Restore all modified files in CvGameCoreDLL_Expansion2
        result = subprocess.run(
            ['git', 'checkout', '--', 'CvGameCoreDLL_Expansion2/'],
            capture_output=True,
            text=True
        )
        
        if result.returncode == 0:
            print(f"✓ Restored files from git HEAD")
            return True
        else:
            print(f"❌ Git rollback failed: {result.stderr}")
            return False
    except Exception as e:
        print(f"❌ Git rollback error: {e}")
        return False

def main():
    """Main function with CRLF workaround"""
    print("=" * 80)
    print("CLANG-TIDY AUTOMATED ANALYSIS WITH CRLF WORKAROUND")
    print("=" * 80)
    print()
    print("This script runs all 14 proven safe checks with:")
    print("  • CRLF→LF conversion (fixes byte offset issues)")
    print("  • C++11→C++03 conversion (nullptr → NULL)")
    print("  • VS2008 compatibility filtering (va_list, NAN, etc.)")
    print()
    
    if not check_prerequisites():
        sys.exit(1)
    
    # Step 1: Convert to LF
    crlf_files = convert_files_to_lf()
    
    # TDD: Verify file sizes after conversion
    test_file = Path("CvGameCoreDLL_Expansion2/CvGame.cpp")
    if test_file.exists():
        size_after_conv = len(test_file.read_bytes())
        print(f"🔍 TDD: CvGame.cpp size after conversion: {size_after_conv} bytes")
    
    # Step 2: Create backups
    backup_files(crlf_files)
    
    # TDD: Verify file sizes after backup
    if test_file.exists():
        size_after_backup = len(test_file.read_bytes())
        print(f"🔍 TDD: CvGame.cpp size after backup: {size_after_backup} bytes")
    
    try:
        # Step 3: Run clang-tidy analysis
        if not run_combined_analysis():
            print("\n⚠️  Analysis failed, restoring from backup...")
            restore_from_backup(crlf_files)
            convert_files_to_crlf(crlf_files)
            sys.exit(1)
        
        # Step 4: Convert back to CRLF
        convert_files_to_crlf(crlf_files)
        
        # Step 5: Clean up backups
        cleanup_backups(crlf_files)
        
        print("\n" + "=" * 80)
        print("✅ CLANG-TIDY ANALYSIS COMPLETED SUCCESSFULLY!")
        print("=" * 80)
        print()
        print("Next steps:")
        print("  1. Review changes: git diff")
        print("  2. Build and test: python3 build_vp_clang_linux.py")
        print("  3. Commit if satisfied: git commit -am 'Apply clang-tidy fixes'")
        print()
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user, restoring from backup...")
        restore_from_backup(crlf_files)
        convert_files_to_crlf(crlf_files)
        sys.exit(130)
    except Exception as e:
        print(f"\n❌ Error: {e}")
        print("Restoring original line endings...")
        convert_files_to_crlf(crlf_files)
        sys.exit(1)

if __name__ == "__main__":
    main()