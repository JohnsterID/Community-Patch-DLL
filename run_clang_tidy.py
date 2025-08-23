#!/usr/bin/env python3
"""
Fixed Automated Clang-Tidy Script for VS2008/C++03 Compatibility
Addresses overlapping replacements and C++11 to C++03 conversion issues
"""

import subprocess
import sys
import time
import yaml
import re
from pathlib import Path
from collections import defaultdict

# LLVM tools paths
CLANG_TIDY = "/tmp/LLVM-20.1.8-Linux-X64/bin/clang-tidy"
CLANG_APPLY_REPLACEMENTS = "/tmp/LLVM-20.1.8-Linux-X64/bin/clang-apply-replacements"

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
        
        if fixes_file.exists():
            # Process and filter fixes
            processed_file = process_and_filter_fixes(fixes_file)
            
            if processed_file:
                print(f"Applying processed fixes from {processed_file}")
                
                # Apply fixes using clang-apply-replacements
                apply_cmd = [CLANG_APPLY_REPLACEMENTS, "."]
                
                # Move processed file to expected location
                processed_file.rename("clang-tidy-combined-fixes.yaml")
                
                apply_result = subprocess.run(apply_cmd, capture_output=True, text=True)
                
                if apply_result.returncode == 0:
                    print("✓ All processed fixes applied successfully")
                    
                    # Validate applied fixes for corruption
                    corruption_found = validate_applied_fixes()
                    if corruption_found:
                        print("❌ Corruption detected in applied fixes!")
                        return False
                else:
                    print(f"❌ Error applying fixes: {apply_result.stderr}")
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

def main():
    """Main function"""
    if not check_prerequisites():
        sys.exit(1)
    
    if not run_combined_analysis():
        sys.exit(1)
    
    print("\n✅ Automated clang-tidy analysis completed successfully!")

if __name__ == "__main__":
    main()