# Modinfo Manager - Complete Modinfo File Management

A comprehensive cross-platform Python script for Windows and Linux that provides complete lifecycle management for Civilization V modinfo files.

## Features

### Core Functionality
- **Hash Updates**: Automatically updates MD5 hashes for changed files
- **New File Detection**: Finds files in your mod directory that aren't in the modinfo
- **Missing File Cleanup**: Removes references to deleted files from modinfo
- **File Reorganization**: Handles when files are moved or reorganized
- **Smart Import Attributes**: Automatically determines correct `import="0"` or `import="1"` values

### Safety & Control
- **Interactive Mode**: Ask for confirmation before making changes
- **Automatic Backups**: Creates timestamped backups before modifications
- **Dry-Run Mode**: Preview all changes without making modifications
- **Comprehensive Logging**: Detailed output of all operations

### Organization
- **File Sorting**: Keeps modinfo files organized alphabetically
- **XML Formatting**: Preserves proper XML structure and formatting
- **ModBuddy Compatibility**: Maintains UTF-8 BOM for compatibility with ModBuddy-generated files
- **Cross-Platform**: Works identically on Windows and Linux

## Requirements

- Python 3.6 or higher
- No additional dependencies (uses only standard library)

## Installation

1. Download `modinfo_manager.py` to your mod directory
2. Make it executable (Linux/Mac): `chmod +x modinfo_manager.py`
3. Run it: `python modinfo_manager.py`

## Usage

### Basic Usage

```bash
# Interactive mode - asks for confirmation before changes
python modinfo_manager.py

# Process specific modinfo file
python modinfo_manager.py "My Mod.modinfo"

# Process specific directory
python modinfo_manager.py /path/to/mod/directory
```

### Advanced Options

```bash
# Preview all changes without making modifications
python modinfo_manager.py --dry-run

# Automatic mode - no user prompts
python modinfo_manager.py --non-interactive

# Skip creating backup files
python modinfo_manager.py --no-backup

# Enable detailed logging
python modinfo_manager.py --verbose

# Combine options
python modinfo_manager.py --dry-run --verbose /path/to/mods
```

### Help

```bash
python modinfo_manager.py --help
```

## 🎯 What It Does

### 1. Hash Updates
- Scans all files referenced in your modinfo
- Calculates current MD5 hashes
- Updates any mismatched hashes
- Reports which files were updated

### 2. New File Detection
- Scans your entire mod directory
- Identifies files not listed in modinfo
- Suggests adding them with appropriate import attributes
- Excludes common non-mod files (`.git`, `.pyc`, etc.)

### 3. Missing File Cleanup
- Identifies files listed in modinfo that no longer exist
- Offers to remove dead references
- Keeps your modinfo clean and accurate

### 4. Smart Import Attributes
- **`import="0"`**: Database files (`.sql`, XML files in `Database Changes/`)
- **`import="1"`**: Game assets (`.lua`, `.dds`, `.mp3`, UI files)
- Automatically determines the correct value for new files

### 5. File Organization
- Sorts all file entries alphabetically
- Maintains consistent XML formatting
- Preserves original XML declaration format

## Example Output

```
Processing: (1) Community Patch (v 143).modinfo
Files in directory: 676
Files in modinfo: 668
Files to add: 3
Files to remove: 0
Files to check: 668

Hash updates needed: 4
Hash mismatch for Assets/FontIcons/CommunityFontIconAtlas.ggxml:
  Current: 97F47A5F2E7A9A39FFD7D7E7A3EED3D6
  Actual:  BAC6136EA854B8C3BA46FBC6CDA6B5DE
Hash mismatch for Core Files/Overrides/EnemyUnitPanel.lua:
  Current: FC0103235E57B56E9760590FE094C23B
  Actual:  CAEC64819051511E3F4C014A03483B3E

New files to add: 3
  + Database Changes/Minors/CoreMinorQuestChanges.sql (import=0)
  + Core Files/New UI/NewFeaturePanel.lua (import=1)
  + Assets/Audio/NewSoundEffect.mp3 (import=1)

Apply these changes to (1) Community Patch (v 143).modinfo? [Y/n]: y
Created backup: (1) Community Patch (v 143).modinfo.backup_20241202_143022
Updated 4 hashes
Added: Database Changes/Minors/CoreMinorQuestChanges.sql (import=0)
Added: Core Files/New UI/NewFeaturePanel.lua (import=1)
Added: Assets/Audio/NewSoundEffect.mp3 (import=1)
Successfully updated (1) Community Patch (v 143).modinfo

Summary:
  Files checked: 668
  Hashes updated: 4
  Files added: 3
  Files removed: 0
  Errors: 0
```

## File Type Detection

The script automatically determines import attributes based on file types and locations:

| File Type | Location | Import Value | Reason |
|-----------|----------|--------------|---------|
| `.sql` | Anywhere | `0` | Database scripts |
| `.xml` | `Database Changes/` | `0` | Database definitions |
| `.xml` | UI directories | `1` | Interface files |
| `.lua` | Anywhere | `1` | Game scripts |
| `.dds` | Anywhere | `1` | Textures/images |
| `.mp3` | Anywhere | `1` | Audio files |
| Others | Anywhere | `1` | Default for assets |

## Safety Features

### Automatic Backups
- Creates timestamped backups before any changes
- Format: `filename.modinfo.backup_YYYYMMDD_HHMMSS`
- Can be disabled with `--no-backup`

### Interactive Confirmation
- Shows exactly what changes will be made
- Asks for user confirmation before proceeding
- Can be disabled with `--non-interactive`

### Dry-Run Mode
- Preview all changes without making modifications
- Perfect for testing or reviewing changes
- Use `--dry-run` flag

### ModBuddy Compatibility
- Preserves UTF-8 BOM (Byte Order Mark) in saved files
- Ensures compatibility with ModBuddy-generated modinfo files
- Maintains consistent file encoding across all mod tools

## Excluded Files

The script automatically excludes these file patterns:
- `*.pyc`, `*.pyo` (Python bytecode)
- `*.git*`, `*.svn*` (Version control)
- `*.DS_Store`, `Thumbs.db` (System files)
- `*.tmp`, `*.temp`, `*.bak`, `*.backup` (Temporary files)
- `*.log` (Log files)
- `*.txt`, `*.md`, `*.rst` (Documentation files)
- `*.civ5proj`, `*.civ5sln`, `*.vcxproj`, `*.sln` (Visual Studio project files)
- `*.exe`, `*.so`, `*.dylib` (Non-game executables)
- `*.zip`, `*.rar`, `*.7z`, `*.tar.gz` (Archive files)
- `*.pdf`, `*.doc`, `*.docx` (Document files)
- `*.modinfo` (Modinfo files themselves)
- Script files (`modinfo_manager.py`, etc.)

**Note**: Game DLL files (like `CvGameCore_Expansion2.dll`) are included as they're required by the game.

## Advanced Usage

### Batch Processing
```bash
# Process all modinfo files in current directory and subdirectories
python modinfo_manager.py --non-interactive --verbose .

# Process multiple mod directories
for mod in /path/to/mods/*/; do
    python modinfo_manager.py --non-interactive "$mod"
done
```

### Integration with Version Control
```bash
# Check what would change before committing
python modinfo_manager.py --dry-run

# Update modinfo files before committing
python modinfo_manager.py --non-interactive

# Commit changes
git add *.modinfo
git commit -m "Update modinfo files"
```

## Comparison with Basic Hash Updater

| Feature | Basic Hash Updater | Modinfo Manager |
|---------|-------------------|-----------------|
| Hash Updates | Yes | Yes |
| New File Detection | No | Yes |
| Missing File Cleanup | No | Yes |
| Interactive Mode | No | Yes |
| Automatic Backups | No | Yes |
| Smart Import Attributes | No | Yes |
| File Sorting | No | Yes |
| File Reorganization | No | Yes |

## Troubleshooting

### Common Issues

1. **"No .modinfo files found"**
   - Check that you're in the correct directory
   - Ensure modinfo files have the `.modinfo` extension

2. **"XML parsing error"**
   - The modinfo file may be corrupted
   - Check the XML syntax and encoding
   - Restore from backup if available

3. **"Permission denied"**
   - Ensure you have write access to the modinfo file
   - On Linux, you may need to adjust file permissions

4. **"File not found" errors**
   - Referenced files may have been moved or deleted
   - Use the script to clean up dead references

### Debug Mode

Use `--verbose` to see detailed information:

```bash
python modinfo_manager.py --verbose --dry-run
```

This shows:
- Which files are being processed
- Hash comparisons for each file
- Detailed error messages
- File paths being checked
- Import attribute decisions

## Migration from Basic Hash Updater

If you're currently using the basic `modinfo_hash_updater.py`:

1. **Backup your modinfo files** (the new script does this automatically)
2. **Run in dry-run mode first**: `python modinfo_manager.py --dry-run`
3. **Review the proposed changes** to ensure they look correct
4. **Run normally**: `python modinfo_manager.py`

The new script is fully backward compatible and provides all the functionality of the basic version plus much more.

## Real-World Examples

This script addresses the gaps identified in Community Patch DLL PRs where manual modinfo updates were needed:

- **PR #11383**: Massive modinfo reorganization with 738 additions, 612 deletions
- **PR #11870**: Hash updates for changed files plus new file additions
- **PR #11932**: File removals, moves, and reorganization

The script would have automated all these manual processes, eliminating human error and saving development time.

## License

This script is licensed under the GNU General Public License v3.0.
See <https://www.gnu.org/licenses/gpl-3.0.html> for details.

## Contributing

Found a bug or have a feature request? This tool is designed to make Civilization V modding easier for everyone.