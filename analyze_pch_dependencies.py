#!/usr/bin/env python3
"""
Analyze header-to-header dependencies within the PCH.

This script finds which headers in the PCH depend on other headers also in the PCH,
so we can add explicit includes BEFORE removing headers from PCH.
"""

import os
import re
from collections import defaultdict

# Path to the project
PROJECT_DIR = "CvGameCoreDLL_Expansion2"
PCH_FILE = os.path.join(PROJECT_DIR, "CvGameCoreDLLPCH.h")

def get_headers_in_pch():
    """Extract list of game headers included in PCH."""
    headers = []
    in_game_section = False
    
    with open(PCH_FILE, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            # Start looking after the Lua include (last framework header)
            if 'CvLuaSupport.h' in line:
                in_game_section = True
                continue
            
            if in_game_section:
                # Stop at "using namespace" or end of file
                if 'using namespace' in line or line.strip().startswith('#endif'):
                    break
                
                # Extract header name from #include "Header.h"
                match = re.match(r'#include\s+"([^"]+)"', line.strip())
                if match:
                    header = match.group(1)
                    headers.append(header)
    
    return headers

def get_includes_from_header(header_path):
    """Extract all #include statements from a header file."""
    includes = []
    
    if not os.path.exists(header_path):
        return includes
    
    try:
        with open(header_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                # Look for #include "Header.h" (local includes)
                match = re.match(r'#include\s+"([^"]+)"', line.strip())
                if match:
                    includes.append(match.group(1))
    except Exception as e:
        print(f"Error reading {header_path}: {e}")
    
    return includes

def build_dependency_graph(headers_in_pch):
    """Build a graph of which PCH headers depend on which other PCH headers."""
    dependencies = defaultdict(list)
    
    for header in headers_in_pch:
        header_path = os.path.join(PROJECT_DIR, header)
        includes = get_includes_from_header(header_path)
        
        # Check which of these includes are also in the PCH
        for included in includes:
            if included in headers_in_pch and included != header:
                dependencies[header].append(included)
    
    return dependencies

def find_headers_to_remove():
    """Find headers that were removed in Phase 1."""
    removed = [
        "CvTreasury.h",
        "CvTechClasses.h",
        "CvPolicyClasses.h",
        "CvBuildingClasses.h",
        "CvProjectClasses.h",
        "CvPromotionClasses.h",
        "CvEmphasisClasses.h",
        "CvBeliefClasses.h",
        "CvReligionClasses.h",
        "CvTradeClasses.h",
        "CvCultureClasses.h",
        "CvNotificationClasses.h",
        "CvCityStrategyAI.h",
        "CvCityCitizens.h",
        "CvCorporationClasses.h",
        "CvContractClasses.h",
    ]
    return removed

def analyze_impact(removed_headers, headers_in_pch, dependencies):
    """Analyze which headers in PCH will be affected by removing headers."""
    
    print("=" * 80)
    print("PCH DEPENDENCY ANALYSIS - Phase 1 Impact")
    print("=" * 80)
    print()
    
    affected_headers = defaultdict(list)
    
    # For each header we're removing, find which headers in PCH depend on it
    for removed in removed_headers:
        for header, deps in dependencies.items():
            if removed in deps and header in headers_in_pch:
                affected_headers[removed].append(header)
    
    print(f"Headers to remove from PCH: {len(removed_headers)}")
    print(f"Headers currently in PCH: {len(headers_in_pch)}")
    print()
    
    # Show headers with no dependents (safe to remove)
    safe_to_remove = []
    needs_fixes = []
    
    for removed in removed_headers:
        if removed in affected_headers and affected_headers[removed]:
            needs_fixes.append(removed)
        else:
            safe_to_remove.append(removed)
    
    print(f"✅ SAFE TO REMOVE ({len(safe_to_remove)} headers):")
    print("   No headers in PCH depend on these")
    print()
    for header in sorted(safe_to_remove):
        print(f"   - {header}")
    print()
    
    print(f"⚠️  NEEDS FIXES ({len(needs_fixes)} headers):")
    print("   These headers are used by other headers in PCH")
    print()
    
    for removed in sorted(needs_fixes):
        print(f"   {removed}:")
        print(f"      Used by {len(affected_headers[removed])} header(s) in PCH:")
        for dependent in sorted(affected_headers[removed]):
            print(f"         - {dependent}")
        print()
    
    return affected_headers

def generate_fix_commands(affected_headers):
    """Generate commands to fix the affected headers."""
    
    if not affected_headers:
        print("✅ No fixes needed!")
        return
    
    print("=" * 80)
    print("FIX COMMANDS")
    print("=" * 80)
    print()
    print("Add these includes to the affected headers:")
    print()
    
    # Group by dependent header (what needs to be fixed)
    fixes_needed = defaultdict(list)
    for removed, dependents in affected_headers.items():
        for dependent in dependents:
            fixes_needed[dependent].append(removed)
    
    for dependent in sorted(fixes_needed.keys()):
        print(f"File: {dependent}")
        print(f"Add these includes after existing #includes:")
        for removed in sorted(fixes_needed[dependent]):
            print(f'   #include "{removed}"')
        print()

def main():
    print("Analyzing PCH dependencies...")
    print()
    
    # Get current state of PCH (after Phase 1 changes)
    headers_in_pch = get_headers_in_pch()
    
    # Get headers we want to remove
    removed_headers = find_headers_to_remove()
    
    # Build dependency graph
    dependencies = build_dependency_graph(headers_in_pch + removed_headers)
    
    # Analyze impact
    affected = analyze_impact(removed_headers, headers_in_pch, dependencies)
    
    # Generate fixes
    generate_fix_commands(affected)
    
    print("=" * 80)
    print("SUMMARY")
    print("=" * 80)
    print()
    print("This analysis helps you:")
    print("1. Identify which headers can be safely removed (no dependencies)")
    print("2. Identify which headers need explicit includes added first")
    print("3. Know exactly which files to modify before removing from PCH")
    print()
    print("Use this BEFORE removing headers from PCH to avoid compile errors!")
    print()

if __name__ == "__main__":
    main()
