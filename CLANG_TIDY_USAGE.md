# Clang-Tidy Usage Guide

## Overview

This guide explains how to use clang-tidy automation for the Community Patch DLL codebase. The system includes custom tools to handle CRLF line endings and UTF-8 encoding issues that break the standard LLVM clang-tidy tools.

## Quick Start

### Analysis Only (Safe - Recommended)

Run clang-tidy for analysis without applying fixes:

```bash
python3 run_clang_tidy.py
```

This will:
- Analyze source files with 14 proven safe checks
- Generate YAML fix files in clang-tidy-fixes/
- Report issues without modifying code

### Applying Fixes (Use with Caution)

Apply fixes using the custom applicator:

```bash
# Generate fixes
python3 run_clang_tidy.py

# Apply fixes with custom tool
python3 apply_yaml_fixes.py clang-tidy-fixes/
```

The custom applicator handles:
- CRLF/LF line ending conversions
- UTF-8 multi-byte character offsets
- Validation and corruption detection
- Automatic backup/restore on failure

## Core Scripts

### run_clang_tidy.py

Main automation script that runs clang-tidy with VS2008/C++03 compatibility filtering.

**Features:**
- Runs multiple checks in parallel
- Converts CRLF to LF before analysis
- Filters out C++11+ suggestions
- Exports compile_commands.json
- Generates YAML fix files

**Usage:**
```bash
# Run all safe checks
python3 run_clang_tidy.py

# Run with custom compile database
python3 run_clang_tidy.py --compile-commands=/path/to/compile_commands.json

# Dry run mode
python3 run_clang_tidy.py --dry-run
```

### apply_yaml_fixes.py

Custom YAML fix applicator with proper CRLF and UTF-8 handling.

**Features:**
- Handles CRLF/LF conversion automatically
- Uses byte offsets (not character offsets)
- Validates changes before writing
- Automatic backup/restore on corruption
- Dry-run mode for testing

**Usage:**
```bash
# Apply all fixes in directory
python3 apply_yaml_fixes.py clang-tidy-fixes/

# Dry run (don't modify files)
python3 apply_yaml_fixes.py clang-tidy-fixes/ --dry-run

# Verbose mode
python3 apply_yaml_fixes.py clang-tidy-fixes/ --verbose
```

## Running Specific Checks

To run a specific check:

```bash
# Modify run_clang_tidy.py, line ~50
SAFE_CHECKS = [
    'readability-isolate-declaration',
    # Add your check here
]
```

Or use clang-tidy directly:

```bash
clang-tidy -checks='readability-isolate-declaration' \
           -p=clang-build/Release \
           CvGameCoreDLL_Expansion2/CvUnit.cpp \
           -export-fixes=fixes.yaml
```

## Running All Checks at Once

The system can safely run all 14 checks simultaneously:

```bash
python3 run_clang_tidy.py
```

**Why this works:**
- CRLF workaround handles large file sets (tested on 156 files)
- Byte offset fix prevents corruption with multi-byte UTF-8
- Replacements are applied end-to-start (no offset shift issues)
- Build validation catches any problems

**Performance:**
- Running 14 checks individually: ~140 minutes
- Running all at once: ~15 minutes (10x speedup)

## Safe Checks List

### Tier 1 - Very Safe (4 checks)

These checks are extremely safe and produce high-quality improvements:

1. **readability-isolate-declaration** - Split multiple declarations
   ```cpp
   // Before: int a, b, c;
   // After:  int a; int b; int c;
   ```

2. **modernize-use-bool-literals** - Use true/false instead of 0/1
   ```cpp
   // Before: bool flag = 0;
   // After:  bool flag = false;
   ```

3. **readability-container-size-empty** - Use .empty() instead of .size() == 0
   ```cpp
   // Before: if (vec.size() == 0)
   // After:  if (vec.empty())
   ```

4. **readability-inconsistent-declaration-parameter-name** - Fix parameter name mismatches
   ```cpp
   // Declaration: void foo(int count);
   // Definition:  void foo(int num) { }
   // After:       void foo(int count) { }
   ```

### Tier 2 - Safe (2 checks)

5. **cppcoreguidelines-init-variables** - Initialize variables (C++03 compatible)
   ```cpp
   // Before: int count;
   // After:  int count = 0;
   ```
   Note: Currently disabled due to false positives in complex cases

6. **readability-string-compare** - Simplify string comparisons
   ```cpp
   // Before: if (str.compare("test") == 0)
   // After:  if (str == "test")
   ```

### Tier 3 - Additional Safe Checks (8 checks)

7. **readability-avoid-return-with-void-value** - Remove redundant returns
8. **readability-redundant-declaration** - Remove redundant forward declarations
9. **readability-redundant-function-ptr-dereference** - Simplify function pointers
10. **readability-redundant-smartptr-get** - Remove unnecessary .get()
11. **readability-redundant-string-cstr** - Remove unnecessary .c_str()
12. **readability-redundant-string-init** - Simplify string initialization
13. **readability-static-accessed-through-instance** - Use Class:: instead of obj.
14. **readability-simplify-boolean-expr** - Simplify boolean expressions

## VS2008 Compatibility Filtering

The automation automatically filters out incompatible modernizations:

**Filtered out:**
- C++11/14/17 features (auto, nullptr, override, etc.)
- Range-based for loops
- Lambda expressions
- Trailing return types
- Uniform initialization

**Conversion applied:**
- C++11 initializers → C++03 assignment syntax
- Example: `int x{0};` becomes `int x = 0;`

## Workflow

### Complete Automation Workflow

1. **Prepare environment:**
   ```bash
   # Build project to generate compile_commands.json
   python3 build_vp_clang_linux.py --export-compile-commands
   ```

2. **Run analysis:**
   ```bash
   python3 run_clang_tidy.py
   ```

3. **Review generated fixes:**
   ```bash
   ls -lh clang-tidy-fixes/
   cat clang-tidy-fixes/CvUnit.cpp.yaml
   ```

4. **Apply fixes (if desired):**
   ```bash
   # Dry run first
   python3 apply_yaml_fixes.py clang-tidy-fixes/ --dry-run
   
   # Apply for real
   python3 apply_yaml_fixes.py clang-tidy-fixes/
   ```

5. **Validate build:**
   ```bash
   python3 build_vp_clang_linux.py --config release
   ```

6. **Verify idempotency:**
   ```bash
   # Should produce no new fixes
   python3 run_clang_tidy.py
   ls clang-tidy-fixes/  # Should be empty or no changes
   ```

## Safety Features

### Backup/Restore System

The applicator automatically creates backups before modifying files:

```
.clang-tidy-backups/
├── CvUnit.cpp.backup
├── CvGame.cpp.backup
└── ...
```

On corruption or validation failure:
- Original files are automatically restored
- Error details are logged
- No changes are committed

### Corruption Detection

The validation system checks for:
- Brace imbalances: `{` vs `}`
- Parenthesis imbalances: `(` vs `)`
- Spurious characters: `);` corruption patterns
- Unexpected line count changes

### Idempotency Testing

Run the automation twice:

```bash
python3 run_clang_tidy.py
python3 apply_yaml_fixes.py clang-tidy-fixes/
python3 run_clang_tidy.py  # Should find no new issues
```

## Troubleshooting

### Files Still Have CRLF

If you see warnings about CRLF in the applicator, the conversion in run_clang_tidy.py may have failed.

**Fix:**
```bash
# Manually convert to LF
find CvGameCoreDLL_Expansion2 -name "*.cpp" -o -name "*.h" | \
    xargs dos2unix

# Then run again
python3 run_clang_tidy.py
```

### Offset Mismatch Errors

If you see "offset mismatch" or corruption:

1. Check for multi-byte UTF-8 characters
2. Verify file encoding is UTF-8
3. Ensure CRLF→LF conversion completed
4. Check file size matches expected

### Build Failures After Applying Fixes

1. Restore from backups: `cp .clang-tidy-backups/* CvGameCoreDLL_Expansion2/`
2. Review the YAML fix that caused issues
3. Report the problematic pattern
4. Disable that specific check

## Performance Tips

1. **Use compilation database:**
   - Pre-generate with `--export-compile-commands`
   - Reuse for multiple runs

2. **Run checks in parallel:**
   - All 14 checks run simultaneously by default
   - Uses available CPU cores

3. **Skip unnecessary files:**
   - Focus on specific directories
   - Exclude generated code

4. **Cache results:**
   - YAML fixes can be reviewed offline
   - Apply in batches

## Advanced Usage

### Custom Check List

Edit `run_clang_tidy.py`:

```python
SAFE_CHECKS = [
    'readability-isolate-declaration',
    'modernize-use-bool-literals',
    # Add custom checks here
]
```

### Integration with Build System

Add to build script:

```python
# After successful build
if args.analyze:
    subprocess.run(['python3', 'run_clang_tidy.py'])
```

### CI/CD Integration

```yaml
# .github/workflows/clang-tidy.yml
- name: Run clang-tidy
  run: |
    python3 build_vp_clang_linux.py --export-compile-commands
    python3 run_clang_tidy.py
    if [ -n "$(ls -A clang-tidy-fixes/)" ]; then
      echo "Clang-tidy found issues"
      exit 1
    fi
```

## See Also

- [CLANG_TIDY_BUGS.md](CLANG_TIDY_BUGS.md) - Known bugs and fixes
- [CLANG_TIDY_VALIDATION.md](CLANG_TIDY_VALIDATION.md) - Test results and metrics
- [BUILD_LINUX.md](BUILD_LINUX.md) - Linux build system documentation
