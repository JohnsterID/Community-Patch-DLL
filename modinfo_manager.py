#!/usr/bin/env python3
"""
Modinfo Manager - Enhanced Modinfo File Management Tool

A comprehensive cross-platform Python script for Windows and Linux that:
- Updates MD5 hashes of referenced files
- Detects and adds new files to modinfo
- Removes references to deleted files
- Handles file reorganization and moves
- Provides interactive mode for user control
- Creates backups for safety

Usage:
    python modinfo_manager.py [options] [path]

Author: JohnsterID
"""

import os
import sys
import hashlib
import argparse
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import List, Tuple, Dict, Optional, Set
import shutil
from datetime import datetime


class ModinfoManager:
    """Enhanced modinfo file management with full file lifecycle support."""
    
    # File extensions that typically use import="0" (database files)
    DATABASE_EXTENSIONS = {'.sql', '.xml'}  # XML can be either, but SQL is always 0
    
    # File extensions that typically use import="1" (game assets)
    ASSET_EXTENSIONS = {'.lua', '.dds', '.mp3', '.ggxml'}
    
    # Files to exclude from auto-detection
    EXCLUDE_PATTERNS = {
        '*.pyc', '*.pyo', '*.git*', '*.svn*', '*.DS_Store', 'Thumbs.db',
        '*.tmp', '*.temp', '*.bak', '*.backup', '*.log',
        '*.txt', '*.md', '*.rst',  # Documentation files
        '*.civ5proj', '*.civ5sln', '*.vcxproj', '*.sln',  # Visual Studio project files
        '*.exe', '*.so', '*.dylib',  # Compiled binaries (DLLs are kept for game mods)
        '*.zip', '*.rar', '*.7z', '*.tar.gz',  # Archive files
        '*.pdf', '*.doc', '*.docx'  # Document files
    }
    
    def __init__(self, verbose: bool = False, dry_run: bool = False, interactive: bool = True, 
                 backup: bool = True, auto_import: bool = True):
        self.verbose = verbose
        self.dry_run = dry_run
        self.interactive = interactive
        self.backup = backup
        self.auto_import = auto_import
        
        # Statistics
        self.files_checked = 0
        self.hashes_updated = 0
        self.files_added = 0
        self.files_removed = 0
        self.errors = []
        
    def log(self, message: str, force: bool = False) -> None:
        """Log a message if verbose mode is enabled or force is True."""
        if self.verbose or force:
            print(message)
    
    def calculate_md5(self, file_path: Path) -> str:
        """Calculate MD5 hash of a file."""
        hash_md5 = hashlib.md5()
        try:
            with open(file_path, "rb") as f:
                for chunk in iter(lambda: f.read(4096), b""):
                    hash_md5.update(chunk)
            return hash_md5.hexdigest().upper()
        except Exception as e:
            self.errors.append(f"Error calculating MD5 for {file_path}: {e}")
            return ""
    
    def should_exclude_file(self, file_path: Path) -> bool:
        """Check if a file should be excluded from modinfo."""
        import fnmatch
        
        file_name = file_path.name.lower()
        
        # Check exclude patterns
        for pattern in self.EXCLUDE_PATTERNS:
            if fnmatch.fnmatch(file_name, pattern.lower()):
                return True
        
        # Exclude the modinfo file itself
        if file_path.suffix.lower() == '.modinfo':
            return True
            
        # Exclude our script files
        if file_name in ['modinfo_manager.py', 'modinfo_hash_updater.py', 'readme_modinfo_updater.md']:
            return True
            
        return False
    
    def determine_import_attribute(self, file_path: Path) -> str:
        """Determine the appropriate import attribute value for a file."""
        ext = file_path.suffix.lower()
        
        # SQL files are always import="0"
        if ext == '.sql':
            return "0"
        
        # Most XML files in Database Changes are import="0"
        if ext == '.xml':
            path_str = str(file_path).replace('\\', '/')
            if 'Database Changes' in path_str or 'database' in path_str.lower():
                return "0"
            else:
                return "1"  # UI XML files are usually import="1"
        
        # Asset files are typically import="1"
        if ext in self.ASSET_EXTENSIONS:
            return "1"
        
        # Default to import="1" for unknown types
        return "1"
    
    def find_all_files(self, mod_dir: Path) -> Set[Path]:
        """Find all files in the mod directory that should be in modinfo."""
        all_files = set()
        
        for file_path in mod_dir.rglob('*'):
            if file_path.is_file() and not self.should_exclude_file(file_path):
                # Make path relative to mod directory
                rel_path = file_path.relative_to(mod_dir)
                all_files.add(rel_path)
        
        return all_files
    
    def find_modinfo_files(self, search_path: Path) -> List[Path]:
        """Find all .modinfo files in the given path and subdirectories."""
        modinfo_files = []
        
        if search_path.is_file() and search_path.suffix.lower() == '.modinfo':
            modinfo_files.append(search_path)
        elif search_path.is_dir():
            for modinfo_file in search_path.rglob('*.modinfo'):
                modinfo_files.append(modinfo_file)
        
        return modinfo_files
    
    def parse_modinfo_file(self, modinfo_path: Path) -> Optional[ET.ElementTree]:
        """Parse a modinfo XML file and return the ElementTree."""
        try:
            tree = ET.parse(modinfo_path)
            return tree
        except ET.ParseError as e:
            self.errors.append(f"XML parsing error in {modinfo_path}: {e}")
            return None
        except Exception as e:
            self.errors.append(f"Error reading {modinfo_path}: {e}")
            return None
    
    def get_file_entries(self, tree: ET.ElementTree) -> List[ET.Element]:
        """Extract all File elements from the modinfo XML."""
        root = tree.getroot()
        files_section = root.find('Files')
        
        if files_section is None:
            return []
        
        return files_section.findall('File')
    
    def create_backup(self, modinfo_path: Path) -> Optional[Path]:
        """Create a backup of the modinfo file."""
        if not self.backup or self.dry_run:
            return None
        
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_path = modinfo_path.with_suffix(f'.modinfo.backup_{timestamp}')
        
        try:
            shutil.copy2(modinfo_path, backup_path)
            self.log(f"Created backup: {backup_path}")
            return backup_path
        except Exception as e:
            self.errors.append(f"Failed to create backup: {e}")
            return None
    
    def ask_user_confirmation(self, message: str, default: bool = True) -> bool:
        """Ask user for confirmation if in interactive mode."""
        if not self.interactive:
            return default
        
        suffix = " [Y/n]: " if default else " [y/N]: "
        response = input(message + suffix).strip().lower()
        
        if not response:
            return default
        
        return response in ['y', 'yes', 'true', '1']
    
    def process_modinfo_file(self, modinfo_path: Path) -> bool:
        """Process a single modinfo file with full file management."""
        self.log(f"\nProcessing: {modinfo_path}", force=True)
        
        tree = self.parse_modinfo_file(modinfo_path)
        if tree is None:
            return False
        
        modinfo_dir = modinfo_path.parent
        
        # Get current file entries from modinfo
        file_entries = self.get_file_entries(tree)
        current_files = {}  # path -> (element, hash)
        
        for file_entry in file_entries:
            file_path_str = file_entry.text
            if file_path_str:
                current_hash = file_entry.get('md5', '').upper()
                current_files[file_path_str] = (file_entry, current_hash)
        
        # Find all files that should be in modinfo
        all_files = self.find_all_files(modinfo_dir)
        all_files_str = {str(f).replace('\\', '/') for f in all_files}
        
        # Identify file changes needed
        files_to_add = all_files_str - set(current_files.keys())
        files_to_remove = set(current_files.keys()) - all_files_str
        files_to_check = set(current_files.keys()) & all_files_str
        
        self.log(f"Files in directory: {len(all_files_str)}")
        self.log(f"Files in modinfo: {len(current_files)}")
        self.log(f"Files to add: {len(files_to_add)}")
        self.log(f"Files to remove: {len(files_to_remove)}")
        self.log(f"Files to check: {len(files_to_check)}")
        
        changes_needed = len(files_to_add) > 0 or len(files_to_remove) > 0
        
        # Check for hash updates in existing files
        hash_updates = []
        for file_path_str in files_to_check:
            file_path = modinfo_dir / file_path_str
            file_entry, current_hash = current_files[file_path_str]
            
            self.files_checked += 1
            
            if not file_path.exists():
                continue
            
            actual_hash = self.calculate_md5(file_path)
            if not actual_hash:
                continue
            
            if current_hash != actual_hash:
                self.log(f"Hash mismatch for {file_path_str}:")
                self.log(f"  Current: {current_hash}")
                self.log(f"  Actual:  {actual_hash}")
                hash_updates.append((file_entry, actual_hash))
            else:
                self.log(f"✓ {file_path_str} - hash OK")
        
        if len(hash_updates) > 0:
            changes_needed = True
        
        # If no changes needed, we're done
        if not changes_needed:
            self.log("No changes needed.", force=True)
            return True
        
        # Show summary of changes
        if len(hash_updates) > 0:
            self.log(f"\nHash updates needed: {len(hash_updates)}", force=True)
        
        if len(files_to_add) > 0:
            self.log(f"\nNew files to add: {len(files_to_add)}", force=True)
            if self.verbose:
                for file_path in sorted(files_to_add):
                    self.log(f"  + {file_path}")
        
        if len(files_to_remove) > 0:
            self.log(f"\nMissing files to remove: {len(files_to_remove)}", force=True)
            if self.verbose:
                for file_path in sorted(files_to_remove):
                    self.log(f"  - {file_path}")
        
        # Ask for confirmation if interactive
        if self.interactive and not self.dry_run:
            if not self.ask_user_confirmation(f"\nApply these changes to {modinfo_path.name}?"):
                self.log("Changes cancelled by user.")
                return True
        
        # Create backup before making changes
        if not self.dry_run:
            self.create_backup(modinfo_path)
        
        # Apply changes
        if not self.dry_run:
            return self._apply_changes(tree, modinfo_path, hash_updates, files_to_add, 
                                     files_to_remove, modinfo_dir)
        else:
            self.log("[DRY RUN] Would apply changes")
            return True
    
    def _apply_changes(self, tree: ET.ElementTree, modinfo_path: Path, 
                      hash_updates: List[Tuple[ET.Element, str]], 
                      files_to_add: Set[str], files_to_remove: Set[str],
                      modinfo_dir: Path) -> bool:
        """Apply all changes to the modinfo file."""
        try:
            root = tree.getroot()
            files_section = root.find('Files')
            
            if files_section is None:
                files_section = ET.SubElement(root, 'Files')
            
            # Apply hash updates
            for file_entry, new_hash in hash_updates:
                file_entry.set('md5', new_hash)
                self.hashes_updated += 1
            
            # Remove missing files
            for file_path_str in files_to_remove:
                for file_entry in files_section.findall('File'):
                    if file_entry.text == file_path_str:
                        files_section.remove(file_entry)
                        self.files_removed += 1
                        self.log(f"Removed: {file_path_str}")
                        break
            
            # Add new files
            for file_path_str in files_to_add:
                file_path = modinfo_dir / file_path_str
                file_hash = self.calculate_md5(file_path)
                if not file_hash:
                    continue
                
                import_attr = self.determine_import_attribute(file_path)
                
                new_entry = ET.SubElement(files_section, 'File')
                new_entry.set('md5', file_hash)
                new_entry.set('import', import_attr)
                new_entry.text = file_path_str
                
                self.files_added += 1
                self.log(f"Added: {file_path_str} (import={import_attr})")
            
            # Sort files alphabetically
            file_elements = files_section.findall('File')
            files_section.clear()
            
            # Sort by file path
            file_elements.sort(key=lambda elem: elem.text or "")
            
            for elem in file_elements:
                files_section.append(elem)
            
            # Save the file
            self._save_modinfo(tree, modinfo_path)
            
            self.log(f"Successfully updated {modinfo_path}")
            return True
            
        except Exception as e:
            self.errors.append(f"Error applying changes to {modinfo_path}: {e}")
            return False
    
    def _save_modinfo(self, tree: ET.ElementTree, modinfo_path: Path) -> None:
        """Save the modinfo file with proper formatting and BOM for ModBuddy compatibility."""
        # Preserve XML formatting
        self._indent_xml(tree.getroot())
        
        # Write with proper XML declaration using double quotes
        xml_str = ET.tostring(tree.getroot(), encoding='unicode')
        
        # Write with UTF-8 BOM for compatibility with ModBuddy-generated files
        with open(modinfo_path, 'w', encoding='utf-8-sig') as f:
            f.write('<?xml version="1.0" encoding="utf-8"?>\n')
            f.write(xml_str)
    
    def _indent_xml(self, elem: ET.Element, level: int = 0) -> None:
        """Add indentation to XML elements for pretty printing."""
        indent = "\n" + level * "  "
        if len(elem):
            if not elem.text or not elem.text.strip():
                elem.text = indent + "  "
            if not elem.tail or not elem.tail.strip():
                elem.tail = indent
            for child in elem:
                self._indent_xml(child, level + 1)
            if not child.tail or not child.tail.strip():
                child.tail = indent
        else:
            if level and (not elem.tail or not elem.tail.strip()):
                elem.tail = indent
    
    def run(self, search_path: Path) -> bool:
        """Main execution method."""
        self.log(f"Searching for modinfo files in: {search_path}")
        
        modinfo_files = self.find_modinfo_files(search_path)
        
        if not modinfo_files:
            print(f"No .modinfo files found in {search_path}")
            return False
        
        self.log(f"Found {len(modinfo_files)} modinfo file(s)")
        
        success = True
        for modinfo_file in modinfo_files:
            if not self.process_modinfo_file(modinfo_file):
                success = False
        
        # Print summary
        print(f"\nSummary:")
        print(f"  Files checked: {self.files_checked}")
        print(f"  Hashes updated: {self.hashes_updated}")
        print(f"  Files added: {self.files_added}")
        print(f"  Files removed: {self.files_removed}")
        print(f"  Errors: {len(self.errors)}")
        
        if self.errors:
            print(f"\nErrors encountered:")
            for error in self.errors:
                print(f"  - {error}")
        
        return success and len(self.errors) == 0


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Comprehensive modinfo file management for Civilization V mods",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python modinfo_manager.py                           # Interactive mode, current directory
  python modinfo_manager.py /path/to/mod              # Process specific mod directory
  python modinfo_manager.py --dry-run                 # Preview all changes
  python modinfo_manager.py --non-interactive         # Auto-apply all changes
  python modinfo_manager.py --no-backup               # Skip backup creation
  python modinfo_manager.py --verbose                 # Show detailed output

Features:
  - Updates MD5 hashes for changed files
  - Detects and adds new files to modinfo
  - Removes references to deleted files
  - Handles file moves and reorganization
  - Creates automatic backups
  - Interactive confirmation for changes
  - Smart import attribute assignment
        """
    )
    
    parser.add_argument(
        'path',
        nargs='?',
        default='.',
        help='Path to search for modinfo files (default: current directory)'
    )
    
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Enable verbose output'
    )
    
    parser.add_argument(
        '-n', '--dry-run',
        action='store_true',
        help='Show what would be changed without making modifications'
    )
    
    parser.add_argument(
        '--non-interactive',
        action='store_true',
        help='Run without user prompts (auto-apply changes)'
    )
    
    parser.add_argument(
        '--no-backup',
        action='store_true',
        help='Skip creating backup files'
    )
    
    parser.add_argument(
        '--version',
        action='version',
        version='Modinfo Manager 2.0'
    )
    
    args = parser.parse_args()
    
    # Convert path to Path object
    search_path = Path(args.path).resolve()
    
    if not search_path.exists():
        print(f"Error: Path '{search_path}' does not exist")
        return 1
    
    # Create manager instance
    manager = ModinfoManager(
        verbose=args.verbose,
        dry_run=args.dry_run,
        interactive=not args.non_interactive,
        backup=not args.no_backup
    )
    
    # Run the manager
    success = manager.run(search_path)
    
    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())