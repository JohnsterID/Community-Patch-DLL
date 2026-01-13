# Clang-Tidy Automation for Community Patch DLL

## Overview

This directory contains tools for automated code quality improvements using clang-tidy on the Community Patch DLL codebase. Due to a fundamental incompatibility between Windows line endings (CRLF) and clang-tidy's offset calculations (LF), we've developed a custom fix applicator.

## 🎯 Quick Start

### Analysis Only (Safe - Recommended)
```bash
# Run clang-tidy for analysis without applying fixes
python3 run_clang_tidy.py
```

### Applying Fixes (Experimental)
```bash
# Run stepped automation with custom applicator
python3 run_clang_tidy_stepped.py
```

⚠️ **Note:** Fix application is experimental due to CRLF/LF offset issues (see below).

## 📁 Files

### Core Scripts
- **`apply_yaml_fixes.py`** - Custom YAML fix applicator with CRLF handling
  - Replaces `clang-apply-replacements` which has CRLF bugs
  - Converts CRLF↔LF during processing
  - Applies fixes from end-to-start to avoid offset shifts
  - ~95% functional, needs final debugging

- **`run_clang_tidy.py`** - Main clang-tidy runner (analysis-only mode)
  - Runs 14 proven safe checks
  - VS2008/C++03 compatibility filtering
  - No automatic fix application (by design)

- **`run_clang_tidy_stepped.py`** - Stepped automation
  - Runs checks one-at-a-time
  - Uses custom applicator
  - Validates after each step

- **`run_clang_tidy_tiered.py`** - Tiered testing
  - Tests checks in groups (Tier 1: safe, Tier 2: moderate, etc.)
  - Used for validation

### Documentation
- **`CORRUPTION_ROOT_CAUSE.md`** - Technical analysis of CRLF/LF issue
  - **READ THIS FIRST** to understand why custom tool was needed
  - Complete evidence and explanation
  - ~329 lines of detailed analysis

## 🔍 The CRLF Problem (TL;DR)

**Root Cause:** Source files use CRLF line endings (Windows), but clang-tidy generates byte offsets assuming LF (Unix).

**Impact:** 
- Every line adds 1 byte offset error (due to `\r`)
- After 650 lines → 650 bytes off
- Fixes applied at wrong locations
- Code corrupted with spurious `);` characters

**Why Both Tools Failed:**
- `clang-apply-replacements` (LLVM's tool) → Doesn't handle CRLF
- Our initial custom tool → Same problem until we added CRLF handling

**Solution:**
```python
# Detect CRLF
has_crlf = b'\r\n' in file_bytes

# Convert to LF (matches clang-tidy offsets)
content = content.replace('\r\n', '\n')

# Apply fixes at correct offsets
# ...

# Convert back to CRLF
content = content.replace('\n', '\r\n')
```

**Status:** Custom tool implements this but needs 1-2 hours of debugging for 100% accuracy.

## 📋 Clang-Tidy Checks Used

We use 14 proven safe checks compatible with VS2008/C++03:

### Tier 1 - Very Safe (4 checks)
1. `readability-isolate-declaration` - Split multiple declarations
2. `modernize-use-bool-literals` - Use true/false instead of 0/1
3. `readability-container-size-empty` - Use `.empty()` instead of `.size() == 0`
4. `readability-inconsistent-declaration-parameter-name` - Fix parameter name mismatches

### Tier 2 - Safe (2 checks)
5. `cppcoreguidelines-init-variables` - Initialize variables (with C++11→C++03 conversion)
6. `readability-string-compare` - Simplify string comparisons

### Tier 3 - Additional Safe Checks (8 checks)
7. `readability-avoid-return-with-void-value` - Remove redundant returns
8. `readability-redundant-declaration` - Remove redundant declarations
9. `readability-redundant-function-ptr-dereference` - Simplify function pointers
10. `readability-redundant-smartptr-get` - Remove unnecessary `.get()`
11. `readability-redundant-string-cstr` - Remove unnecessary `.c_str()`
12. `readability-redundant-string-init` - Simplify string initialization
13. `readability-static-accessed-through-instance` - Use `Class::` instead of `obj.`
14. `readability-simplify-boolean-expr` - Simplify boolean expressions

## 🚀 Usage Examples

### 1. Generate Analysis Report
```bash
# Run analysis only (safe)
python3 run_clang_tidy.py

# Output: clang-tidy-combined-results.txt
```

### 2. View Specific Check Results
```bash
# Filter for specific check
grep "readability-isolate-declaration" clang-tidy-combined-results.txt
```

### 3. Manual Fix Application
```bash
# Generate fixes YAML
python3 run_clang_tidy_stepped.py

# Review the YAML
less step-01-readability-isolate-declaration.yaml

# Apply manually or with custom tool (experimental)
python3 apply_yaml_fixes.py step-01-readability-isolate-declaration.yaml
```

### 4. Test Custom Applicator (Dry Run)
```bash
# Test without modifying files
python3 apply_yaml_fixes.py fixes.yaml --dry-run

# See what would change
python3 apply_yaml_fixes.py fixes.yaml --dry-run --verbose
```

## ⚠️ Important Notes

### VS2008/C++03 Compatibility
- All checks are filtered for C++03 compatibility
- `nullptr` → `NULL` conversion applied
- No C++11+ constructs introduced
- Maintains Visual Studio 2008 compatibility

### Known Limitations
1. **CRLF offset issue** - Custom tool is 95% complete, needs final debugging
2. **Manual review recommended** - Always review changes before committing
3. **Build testing required** - Test with VS2008 after applying fixes
4. **Git tracking** - Use git to review and revert if needed

### Safety Recommendations
1. **Work on a branch** - Never apply directly to main
2. **Commit frequently** - Commit clean state before applying fixes
3. **Test after each check** - Apply and test one check at a time
4. **Use git diff** - Review all changes before committing
5. **Build and test** - Ensure game still works

## 🐛 Troubleshooting

### "Corruption detected" Error
- **Cause:** CRLF/LF offset mismatch
- **Solution:** Custom tool should handle this, but still debugging
- **Workaround:** Convert files to LF first:
  ```bash
  dos2unix CvGameCoreDLL_Expansion2/*.cpp
  # Run clang-tidy
  unix2dos CvGameCoreDLL_Expansion2/*.cpp
  ```

### "Offset out of bounds" Error
- **Cause:** YAML offsets don't match file
- **Solution:** Regenerate YAML from current source
- **Check:** Ensure files haven't changed since YAML was generated

### Compilation Errors After Fixes
- **Cause:** Fix introduced C++11 construct or broke syntax
- **Solution:** Revert with `git checkout -- <file>`
- **Prevention:** Use `--dry-run` first, review diffs carefully

## 📊 Statistics

### From Testing
- **Source files:** 156 C++ files
- **Total lines:** ~1,000,000+
- **Analysis time:** ~10 minutes per check
- **Warnings found:** 1,785 (with 514K suppressed in system headers)

### Known Issues
- **Tier 1 single check test:** Applied 12 fixes to 7 files
- **Result:** Offsets slightly off due to CRLF
- **Status:** Need 1-2 hours to fix offset calculation

## 🔬 Technical Details

See `CORRUPTION_ROOT_CAUSE.md` for:
- Complete CRLF/LF offset analysis
- Evidence of why both tools failed
- Detailed technical explanation
- Byte-level offset tracing
- Solution implementation details

## 📝 Development History

### Investigation Timeline
1. **Initial attempt** - Used `clang-apply-replacements` → Corruption
2. **Hypothesis 1** - Thought it was overlapping fixes → Tested single check → Still corrupted
3. **Hypothesis 2** - Thought it was specific checks → Tested different checks → All corrupted
4. **Root cause found** - Discovered CRLF/LF offset mismatch
5. **Solution built** - Custom applicator with CRLF handling

### Commits
- Initial clang-tidy automation (with bugs)
- Investigation and findings documentation
- Custom YAML applicator creation
- CRLF handling implementation
- Current state: 95% complete

## 🎯 Next Steps

### To Complete Custom Tool (1-2 hours)
1. Debug exact byte offset calculation
2. Handle edge cases in CRLF conversion
3. Test on all 14 checks
4. Validate with compilation

### Alternative: DOS2UNIX Workaround (30 min)
1. Convert all source files to LF
2. Run clang-tidy (offsets now match!)
3. Use standard tools
4. Convert back to CRLF

### Long Term: Report to LLVM
- Document CRLF incompatibility
- Submit bug report
- Help community

## 🤝 Contributing

When working with these tools:
1. **Test thoroughly** - Always use `--dry-run` first
2. **Document changes** - Update this README with findings
3. **Commit atomically** - One check's fixes per commit
4. **Follow authorship** - Use JohnsterID <69278611+JohnsterID@users.noreply.github.com>

## 📚 References

- [LLVM Clang-Tidy Documentation](https://clang.llvm.org/extra/clang-tidy/)
- [LLVM 21.1.8 Release](https://github.com/llvm/llvm-project/releases/tag/llvmorg-21.1.8)
- Community-Patch-DLL-notes.txt - Project coding standards
- GIT_COMMIT_AUTHORSHIP_INSTRUCTIONS.md - Git commit requirements

---

**Last Updated:** 2026-01-13  
**Status:** Custom tool 95% complete, CRLF handling implemented, needs final debugging  
**Maintainer:** JohnsterID
