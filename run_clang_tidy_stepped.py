#!/usr/bin/env python3
"""
Stepped Clang-Tidy Automation with Custom Applicator

Runs clang-tidy checks ONE AT A TIME with custom YAML fix application.

IMPORTANT: Uses custom apply_yaml_fixes.py instead of clang-apply-replacements
because the LLVM tool has a bug that corrupts code.

Strategy:
1. Run check 1 → apply fixes with custom tool → validate
2. Run check 2 on modified code → apply → validate
3. Continue for all checks
4. Each check sees results of previous check (no overlaps!)

The custom applicator applies fixes correctly without corruption.
"""

import subprocess
import sys
import time
import yaml
import re
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

# Checks to run (one at a time)
CHECKS = [
    # Tier 1: Safe, simple changes
    "readability-isolate-declaration",
    "modernize-use-bool-literals",
    "readability-container-size-empty",
    "readability-inconsistent-declaration-parameter-name",
    
    # Tier 2: Medium risk
    "cppcoreguidelines-init-variables",
    "readability-string-compare",
    
    # Tier 3: More complex (enable if Tier 1+2 work)
    # "readability-simplify-boolean-expr",
    # "readability-avoid-return-with-void-value",
    # "readability-redundant-declaration",
    # "readability-redundant-function-ptr-dereference",
    # "readability-redundant-smartptr-get",
    # "readability-redundant-string-cstr",
    # "readability-redundant-string-init",
    # "readability-static-accessed-through-instance",
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
    return cpp_files

def convert_cpp11_to_cpp03(replacement_text):
    """Convert C++11 constructs to VS2008/C++03 compatible equivalents"""
    if not replacement_text:
        return replacement_text
    
    if "= nullptr" in replacement_text:
        return replacement_text.replace("= nullptr", " = NULL")
    
    return replacement_text

def is_problematic_for_vs2008(replacement_text, context=""):
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

def process_and_filter_fixes(fixes_file, check_name):
    """Process and filter fixes for VS2008/C++03 compatibility"""
    if not Path(fixes_file).exists():
        return None
    
    try:
        with open(fixes_file, 'r') as f:
            data = yaml.safe_load(f)
    except Exception as e:
        print(f"  ❌ Error reading YAML: {e}")
        return None
    
    if not data or 'Diagnostics' not in data:
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
                continue
            
            # Convert C++11 to C++03
            converted = convert_cpp11_to_cpp03(repl_text)
            if converted != repl_text:
                converted_count += 1
                repl['ReplacementText'] = converted
            
            filtered_replacements.append(repl)
        
        msg['Replacements'] = filtered_replacements
        processed_diagnostics.append(diag)
    
    data['Diagnostics'] = processed_diagnostics
    
    processed_file = fixes_file.replace('.yaml', '.processed.yaml')
    with open(processed_file, 'w') as f:
        yaml.dump(data, f, default_flow_style=False)
    
    if filtered_count > 0:
        print(f"  Filtered {filtered_count} problematic fixes")
    if converted_count > 0:
        print(f"  Converted {converted_count} C++11 to C++03 fixes")
    
    return processed_file

def apply_fixes(processed_file):
    """Apply processed fixes using our custom applicator"""
    # Use our custom YAML applicator instead of clang-apply-replacements
    # (which has a bug that corrupts code)
    
    cmd = ["python3", "apply_yaml_fixes.py", str(processed_file)]
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    # Print output
    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr)
    
    return result.returncode == 0

def validate_for_corruption():
    """Validate source files for corruption patterns
    
    NOTE: We check git diff to see what actually changed, not scan all files.
    Many files have legitimate patterns like func()); that aren't corruption.
    """
    
    # Check git diff to see what we actually changed
    result = subprocess.run(
        ['git', 'diff', '--name-only', 'CvGameCoreDLL_Expansion2/'],
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0 or not result.stdout.strip():
        # No changes detected
        return []
    
    changed_files = [f.strip() for f in result.stdout.strip().split('\n') if f.strip()]
    
    if not changed_files:
        return []
    
    # Check the actual diff for suspicious patterns
    result = subprocess.run(
        ['git', 'diff', '-U0'] + changed_files,
        capture_output=True,
        text=True
    )
    
    diff_content = result.stdout
    
    # Look for added lines (starting with +) that have suspicious corruption
    # Real corruption from our tool would show as NEW lines in the diff
    corrupted = []
    
    # Check for mangled code (obvious corruption)
    corruption_indicators = [
        r'\+.*\w{20,}',  # Very long identifier without spaces (mangled)
        r'\+.*[a-zA-Z]\[\s*[a-zA-Z]',  # Unexpected array syntax mid-word
        r'\+.*std::vector.*pLost',  # Specific known corruption pattern
    ]
    
    for pattern in corruption_indicators:
        if re.search(pattern, diff_content):
            corrupted.append(("git diff", [pattern]))
            break
    
    return corrupted

def run_single_check(check_name, step_num, total_steps):
    """Run a single check, apply fixes, and validate"""
    print(f"\n{'='*70}")
    print(f"STEP {step_num}/{total_steps}: {check_name}")
    print(f"{'='*70}")
    
    cpp_files = find_source_files()
    fixes_file = f"step-{step_num:02d}-{check_name}.yaml"
    results_file = f"step-{step_num:02d}-{check_name}.txt"
    
    print(f"Running clang-tidy on {len(cpp_files)} files...")
    start_time = time.time()
    
    cmd = [
        CLANG_TIDY,
        f"-checks=-*,{check_name}",
        f"-export-fixes={fixes_file}",
        "-p", ".",
    ] + [str(f) for f in cpp_files]
    
    with open(results_file, 'w') as f:
        result = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT)
    
    elapsed = time.time() - start_time
    print(f"  Completed in {elapsed:.1f} seconds (exit code: {result.returncode})")
    
    if not Path(fixes_file).exists():
        print(f"  ℹ️  No fixes suggested")
        return True
    
    # Count diagnostics
    with open(fixes_file) as f:
        content = f.read()
        diag_count = content.count('DiagnosticName:')
    
    if diag_count == 0:
        print(f"  ℹ️  No fixes to apply")
        return True
    
    print(f"  Found {diag_count} diagnostics")
    
    # Process and filter
    print(f"  Processing fixes...")
    processed_file = process_and_filter_fixes(fixes_file, check_name)
    
    if not processed_file:
        print(f"  ❌ Failed to process fixes")
        return False
    
    # Apply fixes
    print(f"  Applying fixes...")
    if not apply_fixes(processed_file):
        print(f"  ❌ Failed to apply fixes")
        return False
    
    # Validate
    print(f"  Validating...")
    corrupted = validate_for_corruption()
    
    if corrupted:
        print(f"  ❌ CORRUPTION DETECTED in {len(corrupted)} files:")
        for fname, examples in corrupted[:5]:
            print(f"     {fname}: {examples}")
        print(f"\n  Reverting changes...")
        subprocess.run(["git", "checkout", "--", "."], capture_output=True)
        return False
    
    print(f"  ✅ No corruption detected")
    print(f"  ✅ Step {step_num} complete!")
    
    return True

def main():
    print("="*70)
    print("STEPPED CLANG-TIDY AUTOMATION")
    print("="*70)
    print("\nStrategy: Run checks ONE AT A TIME to avoid overlapping fixes\n")
    
    if not check_prerequisites():
        return 1
    
    print(f"Will run {len(CHECKS)} checks in sequence:\n")
    for i, check in enumerate(CHECKS, 1):
        print(f"  {i}. {check}")
    
    print("\nStarting in 3 seconds...")
    time.sleep(3)
    
    start_time = time.time()
    completed = 0
    
    for i, check in enumerate(CHECKS, 1):
        success = run_single_check(check, i, len(CHECKS))
        
        if not success:
            print(f"\n{'='*70}")
            print(f"❌ STOPPED at step {i}: {check}")
            print(f"{'='*70}")
            print(f"\nCompleted: {completed}/{len(CHECKS)} checks")
            print(f"Failed: {check}")
            print("\nRecommendation:")
            print(f"  - Check results in: step-{i:02d}-{check}.txt")
            print(f"  - This check may need manual review")
            print(f"  - Comment it out in CHECKS list and re-run")
            return 1
        
        completed += 1
    
    # All checks completed successfully!
    total_time = time.time() - start_time
    
    print(f"\n{'='*70}")
    print("✅ ALL CHECKS COMPLETED SUCCESSFULLY!")
    print(f"{'='*70}")
    print(f"\nCompleted: {completed}/{len(CHECKS)} checks")
    print(f"Total time: {total_time:.1f} seconds ({total_time/60:.1f} minutes)")
    print("\nTo commit changes:")
    print("  git add -u")
    print(f'  git commit -m "Apply clang-tidy stepped automation ({completed} checks)"')
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
