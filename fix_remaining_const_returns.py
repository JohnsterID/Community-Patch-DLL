#!/usr/bin/env python3

import re
import os
import sys

def fix_const_return_types(file_path):
    """Fix const return type qualifiers in a file"""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading {file_path}: {e}")
        return False
    
    original_content = content
    
    # Pattern 1: virtual const bool/int/etc DLLCALL function() const = 0;
    content = re.sub(
        r'\bvirtual\s+const\s+(bool|int|float|double|char|short|long|unsigned)\s+DLLCALL\s+',
        r'virtual \1 DLLCALL ',
        content
    )
    
    # Pattern 2: virtual const bool/int/etc function() const = 0; (without DLLCALL)
    content = re.sub(
        r'\bvirtual\s+const\s+(bool|int|float|double|char|short|long|unsigned)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(',
        r'virtual \1 \2(',
        content
    )
    
    # Pattern 3: const bool/int/etc function() const; (non-virtual)
    content = re.sub(
        r'\bconst\s+(bool|int|float|double|char|short|long|unsigned)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\([^)]*\)\s*const\s*[;{]',
        lambda m: f'{m.group(1)} {m.group(2)}{m.group(0)[len(f"const {m.group(1)} {m.group(2)}"):]}',
        content
    )
    
    # Pattern 4: const EnumType function() const; (enum return types)
    enum_pattern = r'\bconst\s+([A-Z][A-Za-z0-9_]*(?:Types?|Type|Kind|Mode|State|Status))\s+([A-Za-z_][A-Za-z0-9_]*)\s*\([^)]*\)\s*const\s*[;{]'
    content = re.sub(
        enum_pattern,
        lambda m: f'{m.group(1)} {m.group(2)}{m.group(0)[len(f"const {m.group(1)} {m.group(2)}"):]}',
        content
    )
    
    # Pattern 5: Function definitions - const ReturnType ClassName::function()
    content = re.sub(
        r'\bconst\s+(bool|int|float|double|char|short|long|unsigned)\s+([A-Za-z_][A-Za-z0-9_]*::[A-Za-z_][A-Za-z0-9_]*)\s*\(',
        r'\1 \2(',
        content
    )
    
    # Pattern 6: Function definitions with enum types
    content = re.sub(
        r'\bconst\s+([A-Z][A-Za-z0-9_]*(?:Types?|Type|Kind|Mode|State|Status))\s+([A-Za-z_][A-Za-z0-9_]*::[A-Za-z_][A-Za-z0-9_]*)\s*\(',
        r'\1 \2(',
        content
    )
    
    if content != original_content:
        try:
            with open(file_path, 'w', encoding='utf-8', newline='') as f:
                f.write(content)
            print(f"Fixed: {file_path}")
            return True
        except Exception as e:
            print(f"Error writing {file_path}: {e}")
            return False
    
    return False

def main():
    # Files that need fixing based on warning analysis
    files_to_fix = [
        "CvGameCoreDLLUtil/include/CvDllInterfaces.h",
        "CvGameCoreDLL_Expansion2/CvPlayer.h", 
        "CvGameCoreDLLUtil/include/CvString.h",
        "CvGameCoreDLL_Expansion2/CvDllBuildingInfo.h",
        "CvGameCoreDLL_Expansion2/CvDllPlayer.h",
        "CvWorldBuilderMap/include/CvWorldBuilderMap.h",
        "CvGameCoreDLL_Expansion2/CvDllBuildingInfo.cpp",
        "FirePlace/include/FireWorks/Win32/FDebugHelper.h",
        "CvWorldBuilderMap/include/CvWorldBuilderMapTypeDesc.h",
        "CvWorldBuilderMap/include/CvWorldBuilderMapElementAllocator.h"
    ]
    
    fixed_count = 0
    for file_path in files_to_fix:
        if os.path.exists(file_path):
            if fix_const_return_types(file_path):
                fixed_count += 1
        else:
            print(f"File not found: {file_path}")
    
    print(f"\nFixed {fixed_count} files")

if __name__ == "__main__":
    main()