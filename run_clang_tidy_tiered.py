#!/usr/bin/env python3
"""
Tiered Clang-Tidy Testing Script
Tests checks in groups to identify which ones cause corruption
"""

import subprocess
import sys
import time
import yaml
import re
import argparse
from pathlib import Path
from collections import defaultdict

try:
    import yaml
except ImportError:
    print("Error: PyYAML is required")
    print("Install with: pip3 install pyyaml")
    sys.exit(1)

# LLVM tools paths
CLANG_TIDY = "/tmp/LLVM-21.1.8-Linux-X64/bin/clang-tidy"
CLANG_APPLY_REPLACEMENTS = "/tmp/LLVM-21.1.8-Linux-X64/bin/clang-apply-replacements"

# Tiered check organization
TIER_1_CHECKS = [
    # Low risk: Simple, localized changes
    "readability-isolate-declaration",
    "modernize-use-bool-literals",
    "readability-container-size-empty",
    "readability-inconsistent-declaration-parameter-name",
]

TIER_2_CHECKS = [
    # Medium risk: Adds initializers
    "cppcoreguidelines-init-variables",
    "readability-string-compare",
]

TIER_3_CHECKS = [
    # High risk: Expression manipulation
    "readability-simplify-boolean-expr",
    "readability-avoid-return-with-void-value",
    "readability-redundant-declaration",
    "readability-redundant-function-ptr-dereference",
    "readability-redundant-smartptr-get",
    "readability-redundant-string-cstr",
    "readability-redundant-string-init",
    "readability-static-accessed-through-instance",
]

def check_prerequisites():
    """Check if all required tools are available"""
    print("Checking prerequisites...")
    
    if not Path(CLANG_TIDY).exists():
        print(f"❌ clang-tidy not found at {CLANG_TIDY}")
        return False
    
    if not Path(CLANG_APPLY_REPLACEMENTS).exists():
        print(f"❌ clang-apply-replacements not found at {CLANG_APPLY_REPLACEMENTS}")
        return False
    
    if not Path("compile_commands.json").exists():
        print("❌ compile_commands.json not found")
        print("Run: python3 build_vp_clang_linux.py --export-compile-commands")
        return False
    
    print("✓ All prerequisites satisfied\n")
    return True

def find_source_files():
    """Find all C++ source files"""
    cpp_files = list(Path("CvGameCoreDLL_Expansion2").glob("*.cpp"))
    # For faster testing, use a smaller subset
    # cpp_files = cpp_files[:20]  # Uncomment for quick tests
    print(f"Found {len(cpp_files)} C++ source files")
    return cpp_files

def run_clang_tidy_with_checks(checks, label):
    """Run clang-tidy with specific checks"""
    print(f"\n{'='*60}")
    print(f"Testing: {label}")
    print(f"Checks: {', '.join(checks)}")
    print(f"{'='*60}\n")
    
    cpp_files = find_source_files()
    checks_str = ','.join(checks)
    
    results_file = f"clang-tidy-{label.lower().replace(' ', '-')}-results.txt"
    fixes_file = f"clang-tidy-{label.lower().replace(' ', '-')}-fixes.yaml"
    
    print(f"Running clang-tidy on {len(cpp_files)} files...")
    start_time = time.time()
    
    cmd = [
        CLANG_TIDY,
        f"-checks=-*,{checks_str}",
        f"-export-fixes={fixes_file}",
        "-p", ".",
    ] + [str(f) for f in cpp_files]
    
    with open(results_file, 'w') as f:
        result = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT)
    
    elapsed = time.time() - start_time
    print(f"clang-tidy completed in {elapsed:.1f} seconds (exit code: {result.returncode})")
    
    return results_file, fixes_file, result.returncode

def convert_cpp11_to_cpp03(replacement_text, context=""):
    """Convert C++11 constructs to VS2008/C++03 compatible equivalents"""
    if not replacement_text:
        return replacement_text
    
    if "= nullptr" in replacement_text:
        return replacement_text.replace("= nullptr", " = NULL")
    
    return replacement_text

def is_problematic_for_vs2008(replacement_text, file_path="", context=""):
    """Check if replacement is problematic for VS2008"""
    if not replacement_text:
        return False
    
    # Filter va_list initializations
    if "va_list" in context.lower() and ("= NULL" in replacement_text or "= {}" in replacement_text):
        return True
    
    # Filter NAN assignments
    if "= NAN" in replacement_text:
        return True
    
    # Filter math.h includes
    if "#include <math.h>" in replacement_text:
        return True
    
    # Filter std::to_string (not in VS2008)
    if "std::to_string" in replacement_text:
        return True
    
    return False

def process_and_filter_fixes(fixes_file, label):
    """Process and filter fixes for VS2008/C++03 compatibility"""
    print(f"\nProcessing and filtering fixes for VS2008/C++03 compatibility...")
    
    if not Path(fixes_file).exists():
        print(f"No fixes file generated")
        return None
    
    try:
        with open(fixes_file, 'r') as f:
            data = yaml.safe_load(f)
    except Exception as e:
        print(f"Error reading YAML: {e}")
        return None
    
    if not data or 'Diagnostics' not in data:
        print("No diagnostics found in fixes file")
        return None
    
    filtered_count = 0
    converted_count = 0
    processed_diagnostics = []
    
    for diag in data['Diagnostics']:
        if 'DiagnosticMessage' not in diag:
            continue
        
        msg = diag['DiagnosticMessage']
        if 'Replacements' not in msg:
            processed_diagnostics.append(diag)
            continue
        
        file_path = msg.get('FilePath', '')
        filtered_replacements = []
        
        for repl in msg['Replacements']:
            repl_text = repl.get('ReplacementText', '')
            
            # Check if problematic
            if is_problematic_for_vs2008(repl_text, file_path):
                filtered_count += 1
                print(f"  Filtered problematic: '{repl_text[:50]}' in {Path(file_path).name}")
                continue
            
            # Convert C++11 to C++03
            converted = convert_cpp11_to_cpp03(repl_text)
            if converted != repl_text:
                converted_count += 1
                print(f"  Converted: '= nullptr' → '= NULL' in {Path(file_path).name}")
                repl['ReplacementText'] = converted
            
            filtered_replacements.append(repl)
        
        msg['Replacements'] = filtered_replacements
        processed_diagnostics.append(diag)
    
    data['Diagnostics'] = processed_diagnostics
    
    processed_file = fixes_file.replace('.yaml', '.processed.yaml')
    with open(processed_file, 'w') as f:
        yaml.dump(data, f, default_flow_style=False)
    
    print(f"Filtered {filtered_count} problematic fixes")
    print(f"Converted {converted_count} C++11 to C++03 fixes")
    print(f"Processed fixes saved to: {processed_file}")
    
    return processed_file

def apply_fixes(processed_file):
    """Apply processed fixes"""
    print(f"\nApplying processed fixes from {processed_file}")
    
    cmd = [CLANG_APPLY_REPLACEMENTS, "-format", "-style=file", "."]
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    if result.returncode == 0:
        print("✓ All processed fixes applied successfully")
        return True
    else:
        print(f"❌ Error applying fixes: {result.stderr}")
        return False

def validate_for_corruption():
    """Validate source files for corruption patterns"""
    print(f"\nValidating source files for corruption...")
    
    cpp_files = find_source_files()
    corrupted_files = []
    
    corruption_patterns = [
        r'\w+\(\s*\)\s*\);',  # func());
        r'end\(\s*\)\s*\);',   # end());
        r'plot\(\s*\)\s*\);',  # plot());
    ]
    
    for cpp_file in cpp_files:
        with open(cpp_file, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        corruptions = []
        for pattern in corruption_patterns:
            matches = re.findall(pattern, content)
            if matches:
                corruptions.extend(matches)
        
        if corruptions:
            corrupted_files.append((cpp_file, corruptions))
            print(f"❌ Corruption found in {cpp_file.name}: {corruptions[:5]}")
    
    if corrupted_files:
        print(f"\n❌ Corruption detected in {len(corrupted_files)} files!")
        return False
    else:
        print(f"✓ No corruption detected in {len(cpp_files)} files")
        return True

def main():
    parser = argparse.ArgumentParser(description='Run clang-tidy in tiers to identify problematic checks')
    parser.add_argument('--tier', type=int, choices=[1, 2, 3], 
                        help='Run specific tier (1=safe, 2=+medium, 3=+high-risk)')
    parser.add_argument('--check', type=str,
                        help='Test a single check from Tier 3')
    parser.add_argument('--all', action='store_true',
                        help='Run all tiers sequentially')
    args = parser.parse_args()
    
    if not check_prerequisites():
        return 1
    
    if args.check:
        # Test single check
        if args.check not in TIER_3_CHECKS:
            print(f"Error: {args.check} not in Tier 3 checks")
            print(f"Available: {', '.join(TIER_3_CHECKS)}")
            return 1
        
        checks = TIER_1_CHECKS + TIER_2_CHECKS + [args.check]
        label = f"Tier1+2+{args.check.split('-')[-1]}"
        
    elif args.tier == 1:
        checks = TIER_1_CHECKS
        label = "Tier 1 (Safe)"
        
    elif args.tier == 2:
        checks = TIER_1_CHECKS + TIER_2_CHECKS
        label = "Tier 1+2 (Safe+Medium)"
        
    elif args.tier == 3:
        checks = TIER_1_CHECKS + TIER_2_CHECKS + TIER_3_CHECKS
        label = "All Tiers (Full)"
        
    elif args.all:
        # Run all tiers sequentially
        for tier in [1, 2]:
            print(f"\n{'#'*60}")
            print(f"# TESTING TIER {tier}")
            print(f"{'#'*60}\n")
            
            if tier == 1:
                checks = TIER_1_CHECKS
                label = "Tier 1"
            else:
                checks = TIER_1_CHECKS + TIER_2_CHECKS
                label = "Tier 1+2"
            
            results_file, fixes_file, exit_code = run_clang_tidy_with_checks(checks, label)
            processed_file = process_and_filter_fixes(fixes_file, label)
            
            if processed_file:
                if apply_fixes(processed_file):
                    if not validate_for_corruption():
                        print(f"\n❌ TIER {tier} FAILED - Corruption detected!")
                        print(f"Problematic checks: {checks}")
                        subprocess.run(["git", "checkout", "--", "."])
                        return 1
                    else:
                        print(f"\n✓ TIER {tier} PASSED - No corruption")
                        subprocess.run(["git", "checkout", "--", "."])
                else:
                    print(f"\n❌ TIER {tier} FAILED - Could not apply fixes")
                    return 1
        
        print(f"\n{'='*60}")
        print("All tiers passed! Now testing Tier 3 checks individually...")
        print(f"{'='*60}\n")
        return 0
    else:
        parser.print_help()
        return 1
    
    # Run single test
    results_file, fixes_file, exit_code = run_clang_tidy_with_checks(checks, label)
    processed_file = process_and_filter_fixes(fixes_file, label)
    
    if processed_file:
        if apply_fixes(processed_file):
            if not validate_for_corruption():
                print(f"\n❌ TEST FAILED - Corruption detected!")
                print(f"Reverting changes...")
                subprocess.run(["git", "checkout", "--", "."])
                return 1
            else:
                print(f"\n✓ TEST PASSED - No corruption detected!")
                print(f"\nTo keep these changes:")
                print(f"  git add -u")
                print(f"  git commit -m 'Apply {label} clang-tidy fixes'")
                print(f"\nTo revert:")
                print(f"  git checkout -- .")
                return 0
        else:
            print(f"\n❌ TEST FAILED - Could not apply fixes")
            return 1
    else:
        print(f"\n❌ No fixes to process")
        return 1

if __name__ == "__main__":
    sys.exit(main())
