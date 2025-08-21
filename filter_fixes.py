#!/usr/bin/env python3
"""
Filter problematic clang-tidy fixes for VS2008/C++03 compatibility
"""

import yaml
import re
from pathlib import Path

def filter_problematic_fixes(fixes_file):
    """Filter out problematic fixes that don't work with VS2008/C++03"""
    if not fixes_file.exists():
        print(f"Fixes file not found: {fixes_file}")
        return
    
    print("Filtering problematic fixes for VS2008/C++03 compatibility...")
    
    try:
        with open(fixes_file, 'r') as f:
            data = yaml.safe_load(f)
        
        if not data:
            print("No data in fixes file")
            return
        
        # Problematic patterns to exclude (from clang-tidy_notes.txt)
        problematic_patterns = [
            # cppcoreguidelines-init-variables exclusions
            r'= NULL\s*$',  # Right-side NULL assignments
            r'= 0\s*$',     # Right-side 0 assignments  
            r'= false\s*$', # Right-side false assignments
            r'= NAN',       # NAN assignments
            r'#include <math\.h>', # math.h additions
            # Specific problematic initializations
            r'Connections\s*=\s*0',
            r'VoteCommitmentList\s+\w+\s*=\s*0',
            r'IDInfoVector\s+\w+\s*=\s*0',
            r'LookupTable\s+\w+\s*=\s*0',
            r'TurnData\s+\w+\s*=\s*0',
            r'PlotStatePerTurn\s+\w+\s*=\s*0',
        ]
        
        compiled_patterns = [re.compile(pattern) for pattern in problematic_patterns]
        
        # Process diagnostics
        if 'Diagnostics' in data:
            original_count = len(data['Diagnostics'])
            filtered_diagnostics = []
            filtered_count = 0
            
            for diagnostic in data['Diagnostics']:
                if 'DiagnosticMessage' in diagnostic and 'Replacements' in diagnostic['DiagnosticMessage']:
                    replacements = diagnostic['DiagnosticMessage']['Replacements']
                    filtered_replacements = []
                    
                    for replacement in replacements:
                        replacement_text = replacement.get('ReplacementText', '')
                        file_path = replacement.get('FilePath', '')
                        
                        # Check if this replacement matches any problematic pattern
                        is_problematic = False
                        for pattern in compiled_patterns:
                            if pattern.search(replacement_text):
                                is_problematic = True
                                print(f"  Filtered: '{replacement_text.strip()}' in {Path(file_path).name}")
                                filtered_count += 1
                                break
                        
                        if not is_problematic:
                            filtered_replacements.append(replacement)
                    
                    # Update the diagnostic with filtered replacements
                    diagnostic['DiagnosticMessage']['Replacements'] = filtered_replacements
                
                filtered_diagnostics.append(diagnostic)
            
            # Update the data with filtered diagnostics
            data['Diagnostics'] = filtered_diagnostics
            
            print(f"Filtered {filtered_count} problematic fixes")
            print(f"Processed {original_count} diagnostics")
        
        # Write filtered fixes back
        filtered_file = fixes_file.with_suffix('.filtered.yaml')
        with open(filtered_file, 'w') as f:
            yaml.dump(data, f, default_flow_style=False)
        
        print(f"Filtered fixes saved to: {filtered_file}")
        return filtered_file
        
    except Exception as e:
        print(f"Error filtering fixes: {e}")
        return fixes_file

if __name__ == '__main__':
    fixes_file = Path('clang-tidy-combined-fixes.yaml')
    filter_problematic_fixes(fixes_file)