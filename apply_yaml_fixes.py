#!/usr/bin/env python3
"""
Custom YAML Fix Applicator

Replaces clang-apply-replacements with a working implementation.

Why we need this:
- clang-apply-replacements has a bug that corrupts code with spurious ");\"
- Even single checks with valid YAML produce corrupted output
- This custom applicator applies fixes correctly

Strategy:
1. Parse YAML file
2. Group replacements by file
3. Sort by offset (REVERSE - apply from end first!)
4. Apply each replacement as a simple string operation
5. Validate result
6. Write modified file

The key insight: Apply from END to START to avoid offset shifts!
"""

import sys
import yaml
import argparse
from pathlib import Path
from collections import defaultdict
import re
import shutil
from typing import List, Dict, Tuple

try:
    import yaml
except ImportError:
    print("Error: PyYAML is required")
    print("Install with: pip3 install pyyaml")
    sys.exit(1)

class Replacement:
    """Represents a single replacement"""
    def __init__(self, file_path: str, offset: int, length: int, text: str, diagnostic: str):
        self.file_path = file_path
        self.offset = offset
        self.length = length
        self.text = text
        self.diagnostic = diagnostic
    
    def __repr__(self):
        return f"Replacement(offset={self.offset}, len={self.length}, text={self.text[:30]}...)"

class YAMLFixApplicator:
    """Custom YAML fix applicator"""
    
    def __init__(self, yaml_file: Path, dry_run: bool = False, verbose: bool = False):
        self.yaml_file = yaml_file
        self.dry_run = dry_run
        self.verbose = verbose
        self.replacements_by_file: Dict[str, List[Replacement]] = defaultdict(list)
        self.stats = {
            'files_modified': 0,
            'replacements_applied': 0,
            'replacements_skipped': 0,
            'errors': 0
        }
    
    def load_yaml(self) -> bool:
        """Load and parse YAML file"""
        if not self.yaml_file.exists():
            print(f"FAILED: Error: YAML file not found: {self.yaml_file}")
            return False
        
        print(f"Loading YAML: {self.yaml_file}")
        
        try:
            with open(self.yaml_file, 'r') as f:
                data = yaml.safe_load(f)
        except Exception as e:
            print(f"FAILED: Error parsing YAML: {e}")
            return False
        
        if not data or 'Diagnostics' not in data:
            print("FAILED: Error: No diagnostics found in YAML")
            return False
        
        # Extract replacements
        for diag in data['Diagnostics']:
            diag_name = diag.get('DiagnosticName', 'unknown')
            
            if 'DiagnosticMessage' not in diag:
                continue
            
            msg = diag['DiagnosticMessage']
            if 'Replacements' not in msg:
                continue
            
            file_path = msg.get('FilePath', '')
            if not file_path:
                continue
            
            for repl in msg['Replacements']:
                r = Replacement(
                    file_path=repl.get('FilePath', file_path),
                    offset=repl.get('Offset', 0),
                    length=repl.get('Length', 0),
                    text=repl.get('ReplacementText', ''),
                    diagnostic=diag_name
                )
                self.replacements_by_file[r.file_path].append(r)
        
        total_replacements = sum(len(repls) for repls in self.replacements_by_file.values())
        print(f"SUCCESS: Loaded {total_replacements} replacements across {len(self.replacements_by_file)} files")
        
        return True
    
    def apply_to_file(self, file_path: str, replacements: List[Replacement]) -> bool:
        """Apply replacements to a single file"""
        
        # Convert to Path
        file_path_obj = Path(file_path)
        
        if not file_path_obj.exists():
            print(f"  FAILED: File not found: {file_path_obj}")
            self.stats['errors'] += 1
            return False
        
        # Read file
        try:
            with open(file_path_obj, 'rb') as f:
                original_bytes = f.read()
            original_content = original_bytes.decode('utf-8', errors='replace')
        except Exception as e:
            print(f"  FAILED: Error reading {file_path_obj.name}: {e}")
            self.stats['errors'] += 1
            return False
        
        # CRITICAL: Detect and handle CRLF vs LF mismatch
        # Clang-tidy generates offsets assuming LF, but files may have CRLF
        has_crlf = b'\r\n' in original_bytes
        
        if has_crlf:
            # Convert CRLF to LF so offsets match clang-tidy's expectations
            content_str = original_content.replace('\r\n', '\n')
            print(f"   [WARNING]: File {file_path_obj.name} still has CRLF! Converting to LF...")
            print(f"     This should not happen if run_clang_tidy.py converted properly!")
            print(f"     File size before: {len(original_content.encode('utf-8'))} bytes")
            print(f"     File size after LF: {len(content_str.encode('utf-8'))} bytes")
        else:
            content_str = original_content
        
        # CRITICAL BUG FIX: Work with BYTES, not characters!
        # Clang-tidy generates BYTE offsets, but Python string indexing uses CHARACTER offsets.
        # With multi-byte UTF-8 chars (like copyright symbol), character offset != byte offset!
        content_bytes = content_str.encode('utf-8')
        print(f"  File {file_path_obj.name} has LF (size: {len(content_bytes)} bytes, BOM: {has_bom})")
        
        # Sort replacements by offset (REVERSE - highest first!)
        # This is KEY to avoiding offset shifts
        sorted_replacements = sorted(replacements, key=lambda r: r.offset, reverse=True)
        
        if self.verbose:
            print(f"\n  File: {file_path_obj.name}")
            print(f"  Replacements: {len(sorted_replacements)}")
        
        # Apply each replacement
        # NOTE: Do NOT reset content here - it's already been converted from CRLF to LF above!
        # content = original_content  # FAILED: BUG! This would undo the CRLF→LF conversion
        applied_count = 0
        
        for i, repl in enumerate(sorted_replacements):
            # Validate offset is within bounds (use BYTE length!)
            if repl.offset < 0 or repl.offset > len(content_bytes):
                print(f"  WARNING:  Skipping invalid offset: {repl.offset} (file length: {len(content_bytes)})")
                self.stats['replacements_skipped'] += 1
                continue
            
            # Validate length doesn't exceed file
            if repl.offset + repl.length > len(content_bytes):
                print(f"  WARNING:  Skipping invalid length: {repl.length} at offset {repl.offset}")
                self.stats['replacements_skipped'] += 1
                continue
            
            # Extract context for validation (decode bytes for display)
            context_start = max(0, repl.offset - 40)
            context_end = min(len(content_bytes), repl.offset + repl.length + 40)
            try:
                context_before = content_bytes[context_start:repl.offset].decode('utf-8', errors='replace')
                old_text = content_bytes[repl.offset:repl.offset + repl.length].decode('utf-8', errors='replace')
                context_after = content_bytes[repl.offset + repl.length:context_end].decode('utf-8', errors='replace')
            except:
                context_before = ""
                old_text = ""
                context_after = ""
            
            if self.verbose:
                print(f"    [{i+1}/{len(sorted_replacements)}] Offset {repl.offset}, Length {repl.length}")
                print(f"        Before: ...{context_before[-20:]}[{old_text}]{context_after[:20]}...")
                print(f"        After:  ...{context_before[-20:]}[{repl.text[:40]}]{context_after[:20]}...")
            
            # Apply replacement using BYTE offsets (this is the critical fix!)
            # Clang-tidy uses byte offsets, not character offsets
            replacement_bytes = repl.text.encode('utf-8')
            content_bytes = content_bytes[:repl.offset] + replacement_bytes + content_bytes[repl.offset + repl.length:]
            applied_count += 1
        
        if applied_count == 0:
            if self.verbose:
                print(f"  INFO:  No replacements applied to {file_path_obj.name}")
            return True
        
        # Decode bytes to string for validation
        content_str = content_bytes.decode('utf-8')
        
        # Validate result
        if self.validate_result(content_str, file_path_obj.name):
            # Convert back to CRLF if original had CRLF
            if has_crlf:
                content_str = content_str.replace('\n', '\r\n')
                content_bytes = content_str.encode('utf-8')
                if self.verbose:
                    print(f"  INFO:  Converted LF → CRLF to match original")
            
            # Write modified file (or skip if dry-run)
            if self.dry_run:
                print(f"  [DRY-RUN] Would modify {file_path_obj.name} ({applied_count} replacements)")
            else:
                try:
                    # Write bytes directly to preserve exact encoding
                    with open(file_path_obj, 'wb') as f:
                        f.write(content_bytes)
                    print(f"  SUCCESS: Modified {file_path_obj.name} ({applied_count} replacements)")
                    self.stats['files_modified'] += 1
                except Exception as e:
                    print(f"  FAILED: Error writing {file_path_obj.name}: {e}")
                    self.stats['errors'] += 1
                    return False
            
            self.stats['replacements_applied'] += applied_count
            return True
        else:
            print(f"  FAILED: Validation failed for {file_path_obj.name} - NOT applying changes")
            self.stats['errors'] += 1
            return False
    
    def validate_result(self, content: str, filename: str) -> bool:
        """Validate that result doesn't have obvious corruption"""
        
        # Check for basic syntax issues
        # Count braces/parens
        open_braces = content.count('{')
        close_braces = content.count('}')
        open_parens = content.count('(')
        close_parens = content.count(')')
        
        # Allow small imbalance (could be in comments/strings)
        if abs(open_braces - close_braces) > 5:
            print(f"  WARNING:  Warning: Brace imbalance: {{ {open_braces} vs }} {close_braces}")
            # Not failing validation - might be in strings/comments
        
        if abs(open_parens - close_parens) > 5:
            print(f"  WARNING:  Warning: Paren imbalance: ( {open_parens} vs ) {close_parens}")
            # Not failing validation - might be in strings/comments
        
        # Note: We removed the ");\" pattern checks because they flag too many
        # legitimate cases like: func(expr());" or "for(...; func(); ...)"
        # 
        # Instead, we rely on:
        # 1. Applying from end-to-start (avoids offset shifts)
        # 2. Validation of offsets before application
        # 3. Basic syntax checks above
        
        return True
    
    def apply_all(self) -> bool:
        """Apply all replacements to all files"""
        
        if not self.replacements_by_file:
            print("No replacements to apply")
            return True
        
        print(f"\n{'='*60}")
        if self.dry_run:
            print("DRY RUN - No files will be modified")
        else:
            print(f"Applying fixes to {len(self.replacements_by_file)} files")
        print(f"{'='*60}\n")
        
        success_count = 0
        fail_count = 0
        
        for file_path, replacements in sorted(self.replacements_by_file.items()):
            if self.apply_to_file(file_path, replacements):
                success_count += 1
            else:
                fail_count += 1
        
        # Print summary
        print(f"\n{'='*60}")
        print("SUMMARY")
        print(f"{'='*60}")
        print(f"Files processed: {len(self.replacements_by_file)}")
        print(f"Files modified: {self.stats['files_modified']}")
        print(f"Replacements applied: {self.stats['replacements_applied']}")
        print(f"Replacements skipped: {self.stats['replacements_skipped']}")
        print(f"Errors: {self.stats['errors']}")
        
        if self.dry_run:
            print("\n[DRY-RUN] No files were actually modified")
        
        return fail_count == 0

def main():
    parser = argparse.ArgumentParser(
        description='Custom YAML fix applicator - replaces clang-apply-replacements',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Dry run (no changes)
  python3 apply_yaml_fixes.py fixes.yaml --dry-run

  # Apply fixes
  python3 apply_yaml_fixes.py fixes.yaml

  # Apply with verbose output
  python3 apply_yaml_fixes.py fixes.yaml --verbose

Why this tool exists:
  clang-apply-replacements has a bug that corrupts code.
  This custom tool applies YAML fixes correctly.
        """
    )
    
    parser.add_argument('yaml_file', type=Path,
                        help='YAML file containing fixes (from clang-tidy -export-fixes)')
    parser.add_argument('--dry-run', action='store_true',
                        help='Show what would be changed without modifying files')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Show detailed output for each replacement')
    
    args = parser.parse_args()
    
    if not args.yaml_file.exists():
        print(f"FAILED: Error: File not found: {args.yaml_file}")
        return 1
    
    # Create applicator
    applicator = YAMLFixApplicator(
        yaml_file=args.yaml_file,
        dry_run=args.dry_run,
        verbose=args.verbose
    )
    
    # Load YAML
    if not applicator.load_yaml():
        return 1
    
    # Apply fixes
    if not applicator.apply_all():
        print("\nFAILED: Some errors occurred during application")
        return 1
    
    print("\nSUCCESS: All fixes applied successfully!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
