#!/usr/bin/env python3
"""
Script to collate and analyze Clang Static Analyzer results from plist-multi-file output.
Processes both Debug and Release build results.
"""

import os
import sys
import glob
import xml.etree.ElementTree as ET
from pathlib import Path
from collections import defaultdict, Counter
import json

def parse_plist_file(plist_path):
    """Parse a single plist file and extract diagnostics."""
    try:
        tree = ET.parse(plist_path)
        root = tree.getroot()
        
        # Find the diagnostics array
        diagnostics = []
        
        # Navigate to the main dict
        main_dict = root.find('dict')
        if main_dict is not None:
            # Find diagnostics key and its corresponding array
            elements = list(main_dict)
            for i, elem in enumerate(elements):
                if elem.tag == 'key' and elem.text == 'diagnostics':
                    # The next element should be the diagnostics array
                    if i + 1 < len(elements) and elements[i + 1].tag == 'array':
                        diag_array = elements[i + 1]
                        # Each dict in the array is a diagnostic
                        for diag_dict in diag_array.findall('dict'):
                            diagnostic = parse_diagnostic(diag_dict)
                            if diagnostic:
                                diagnostics.append(diagnostic)
                        break
        
        return diagnostics
    except Exception as e:
        print(f"Error parsing {plist_path}: {e}")
        return []

def parse_diagnostic(diag_dict):
    """Parse a single diagnostic from the plist."""
    diagnostic = {}
    
    # Parse key-value pairs in the diagnostic dict
    elements = list(diag_dict)
    i = 0
    while i < len(elements):
        if elements[i].tag == 'key':
            key_text = elements[i].text
            # The next element should be the value
            if i + 1 < len(elements):
                value_elem = elements[i + 1]
                if value_elem.tag == 'string':
                    diagnostic[key_text] = value_elem.text
                elif value_elem.tag == 'integer':
                    diagnostic[key_text] = int(value_elem.text) if value_elem.text else 0
                elif value_elem.tag == 'array':
                    # For arrays, we'll just note that it exists
                    diagnostic[key_text] = f"array_with_{len(value_elem)}_elements"
                i += 1  # Skip the value element
        i += 1
    
    return diagnostic

def get_file_info(plist_path):
    """Extract file information from plist path."""
    path_obj = Path(plist_path)
    return {
        'filename': path_obj.stem,
        'size_bytes': path_obj.stat().st_size,
        'size_kb': round(path_obj.stat().st_size / 1024, 1)
    }

def analyze_plist_directory(plist_dir, build_type):
    """Analyze all plist files in a directory."""
    plist_pattern = os.path.join(plist_dir, "*.plist")
    plist_files = glob.glob(plist_pattern)
    
    print(f"\n=== {build_type} Build Analysis ===")
    print(f"Found {len(plist_files)} plist files in {plist_dir}")
    
    all_diagnostics = []
    file_stats = []
    warning_types = Counter()
    files_with_warnings = []
    
    total_size = 0
    
    for plist_file in sorted(plist_files):
        file_info = get_file_info(plist_file)
        total_size += file_info['size_bytes']
        
        diagnostics = parse_plist_file(plist_file)
        
        if diagnostics:
            files_with_warnings.append({
                'file': file_info['filename'],
                'warning_count': len(diagnostics),
                'size_kb': file_info['size_kb']
            })
            
            for diag in diagnostics:
                if 'type' in diag:
                    warning_types[diag['type']] += 1
                elif 'category' in diag:
                    warning_types[diag['category']] += 1
                else:
                    warning_types['Unknown'] += 1
        
        file_stats.append({
            'file': file_info['filename'],
            'size_kb': file_info['size_kb'],
            'warning_count': len(diagnostics)
        })
        
        all_diagnostics.extend(diagnostics)
    
    # Sort files by warning count (descending)
    files_with_warnings.sort(key=lambda x: x['warning_count'], reverse=True)
    
    print(f"Total plist files size: {total_size / 1024:.1f} KB")
    print(f"Total warnings found: {len(all_diagnostics)}")
    print(f"Files with warnings: {len(files_with_warnings)}")
    
    # Top warning types
    print(f"\nTop Warning Types:")
    for warning_type, count in warning_types.most_common(10):
        print(f"  {warning_type}: {count}")
    
    # Files with most warnings
    print(f"\nFiles with Most Warnings:")
    for file_info in files_with_warnings[:10]:
        print(f"  {file_info['file']}: {file_info['warning_count']} warnings ({file_info['size_kb']} KB)")
    
    return {
        'build_type': build_type,
        'total_files': len(plist_files),
        'total_size_kb': total_size / 1024,
        'total_warnings': len(all_diagnostics),
        'files_with_warnings': len(files_with_warnings),
        'warning_types': dict(warning_types),
        'top_warning_files': files_with_warnings[:10],
        'all_diagnostics': all_diagnostics
    }

def extract_critical_warnings(diagnostics):
    """Extract critical warnings that should be prioritized."""
    critical_types = [
        'Called C++ object pointer is null',
        'Dereference of null pointer',
        'Division by zero',
        'Garbage return value',
        'Use-after-free',
        'Free \'alloca()\'',
        'Undefined or garbage value returned to caller'
    ]
    
    critical_warnings = []
    for diag in diagnostics:
        diag_type = diag.get('type', '')
        if diag_type in critical_types:
            critical_warnings.append(diag)
    
    return critical_warnings

def generate_summary_report(debug_results, release_results):
    """Generate a comprehensive summary report."""
    print("\n" + "="*80)
    print("CLANG STATIC ANALYZER SUMMARY REPORT")
    print("LLVM 20.1.8 with --analyzer-output plist-multi-file")
    print("="*80)
    
    print(f"\nBUILD COMPARISON:")
    print(f"{'Metric':<30} {'Debug':<15} {'Release':<15}")
    print("-" * 60)
    print(f"{'Total Files':<30} {debug_results['total_files']:<15} {release_results['total_files']:<15}")
    print(f"{'Total Size (KB)':<30} {debug_results['total_size_kb']:<15.1f} {release_results['total_size_kb']:<15.1f}")
    print(f"{'Total Warnings':<30} {debug_results['total_warnings']:<15} {release_results['total_warnings']:<15}")
    print(f"{'Files with Warnings':<30} {debug_results['files_with_warnings']:<15} {release_results['files_with_warnings']:<15}")
    
    # Critical warnings analysis
    debug_critical = extract_critical_warnings(debug_results['all_diagnostics'])
    release_critical = extract_critical_warnings(release_results['all_diagnostics'])
    
    print(f"\nCRITICAL WARNINGS ANALYSIS:")
    print(f"Debug Critical Warnings: {len(debug_critical)}")
    print(f"Release Critical Warnings: {len(release_critical)}")
    
    # Show critical warning types
    debug_critical_types = Counter()
    release_critical_types = Counter()
    
    for diag in debug_critical:
        diag_type = diag.get('type', diag.get('category', 'Unknown'))
        debug_critical_types[diag_type] += 1
    
    for diag in release_critical:
        diag_type = diag.get('type', diag.get('category', 'Unknown'))
        release_critical_types[diag_type] += 1
    
    print(f"\nTop Critical Warning Types (Debug):")
    for warning_type, count in debug_critical_types.most_common(5):
        print(f"  {warning_type}: {count}")
    
    print(f"\nTop Critical Warning Types (Release):")
    for warning_type, count in release_critical_types.most_common(5):
        print(f"  {warning_type}: {count}")
    
    # Files with highest warning density
    print(f"\nHIGHEST WARNING DENSITY FILES:")
    print(f"{'File':<40} {'Warnings':<10} {'Size(KB)':<10} {'Density':<10}")
    print("-" * 70)
    
    combined_files = {}
    for file_info in debug_results['top_warning_files']:
        combined_files[file_info['file']] = file_info
    
    for file_name, file_info in sorted(combined_files.items(), 
                                     key=lambda x: x[1]['warning_count'] / max(x[1]['size_kb'], 1), 
                                     reverse=True)[:10]:
        density = file_info['warning_count'] / max(file_info['size_kb'], 1)
        print(f"{file_name:<40} {file_info['warning_count']:<10} {file_info['size_kb']:<10.1f} {density:<10.2f}")

def save_detailed_results(debug_results, release_results, output_file):
    """Save detailed results to JSON file."""
    combined_results = {
        'analysis_info': {
            'analyzer': 'Clang Static Analyzer',
            'version': 'LLVM 20.1.8',
            'options': '--analyzer-output plist-multi-file',
            'timestamp': None  # Could add timestamp here
        },
        'debug_build': debug_results,
        'release_build': release_results
    }
    
    # Remove the large diagnostics arrays to keep file manageable
    combined_results['debug_build']['all_diagnostics'] = f"{len(debug_results['all_diagnostics'])} diagnostics (removed for brevity)"
    combined_results['release_build']['all_diagnostics'] = f"{len(release_results['all_diagnostics'])} diagnostics (removed for brevity)"
    
    with open(output_file, 'w') as f:
        json.dump(combined_results, f, indent=2)
    
    print(f"\nDetailed results saved to: {output_file}")

def create_critical_warnings_report(debug_results, release_results):
    """Create a detailed report of critical warnings."""
    debug_critical = extract_critical_warnings(debug_results['all_diagnostics'])
    release_critical = extract_critical_warnings(release_results['all_diagnostics'])
    
    print(f"\n" + "="*80)
    print("CRITICAL WARNINGS DETAILED REPORT")
    print("="*80)
    
    print(f"\nDEBUG BUILD CRITICAL WARNINGS ({len(debug_critical)} total):")
    print("-" * 50)
    
    critical_by_file = defaultdict(list)
    for diag in debug_critical:
        # Try to extract file information from the diagnostic
        file_info = "Unknown file"
        if 'issue_context' in diag:
            file_info = diag['issue_context']
        critical_by_file[file_info].append(diag)
    
    for file_name, warnings in sorted(critical_by_file.items()):
        print(f"\n{file_name}:")
        for warning in warnings:
            warning_type = warning.get('type', 'Unknown')
            description = warning.get('description', 'No description')
            print(f"  - {warning_type}: {description}")
    
    if len(debug_critical) != len(release_critical):
        print(f"\nWARNING: Debug and Release builds have different numbers of critical warnings!")
        print(f"Debug: {len(debug_critical)}, Release: {len(release_critical)}")
    
    return debug_critical, release_critical

def main():
    """Main function to analyze plist results."""
    debug_dir = "/workspace/Community-Patch-DLL/clang-build/Debug/CvGameCoreDLL_Expansion2"
    release_dir = "/workspace/Community-Patch-DLL/clang-build/Release/CvGameCoreDLL_Expansion2"
    
    if not os.path.exists(debug_dir):
        print(f"Error: Debug directory not found: {debug_dir}")
        return 1
    
    if not os.path.exists(release_dir):
        print(f"Error: Release directory not found: {release_dir}")
        return 1
    
    # Analyze both builds
    debug_results = analyze_plist_directory(debug_dir, "Debug")
    release_results = analyze_plist_directory(release_dir, "Release")
    
    # Generate summary report
    generate_summary_report(debug_results, release_results)
    
    # Generate critical warnings report
    create_critical_warnings_report(debug_results, release_results)
    
    # Save detailed results
    output_file = "/workspace/Community-Patch-DLL/static_analysis_results.json"
    save_detailed_results(debug_results, release_results, output_file)
    
    return 0

if __name__ == "__main__":
    sys.exit(main())