# Code Analysis Tools

This document describes the automated code analysis and quality tools included in the Linux build system.

---

## Overview

The build system includes several tools for code quality analysis:

| Tool | Purpose | Output |
|------|---------|--------|
| **clang-tidy** | Automated code quality checks | Fixed code + YAML reports |
| **Static Analyzer** | Deep bug detection | Plist files with warnings |
| **Plist Analyzer** | Aggregate analysis results | JSON summary + reports |
| **Fix Filter** | VS2008 compatibility filtering | Clean YAML fixes |

---

## Tool 1: Clang-Tidy Automation

### What It Does

Runs automated code quality checks with VS2008/C++03 compatibility filtering.

**Checks Applied (14 proven checks):**
1. `readability-isolate-declaration` - Separate variable declarations
2. `cppcoreguidelines-init-variables` - Initialize variables
3. `readability-inconsistent-declaration-parameter-name` - Fix parameter names
4. `modernize-use-bool-literals` - Use true/false not 1/0
5. `readability-simplify-boolean-expr` - Simplify boolean expressions
6. `readability-container-size-empty` - Use `.empty()` not `.size() == 0`
7. `readability-string-compare` - Simplify string comparisons
8. `readability-avoid-return-with-void-value` - Fix void returns
9. `readability-redundant-declaration` - Remove duplicate declarations
10. `readability-redundant-function-ptr-dereference` - Clean function pointers
11. `readability-redundant-smartptr-get` - Clean smart pointer usage
12. `readability-redundant-string-cstr` - Remove unnecessary `.c_str()`
13. `readability-redundant-string-init` - Clean string initialization
14. `readability-static-accessed-through-instance` - Fix static access

### Prerequisites

```bash
# Install PyYAML
pip3 install -r requirements.txt

# Generate compile_commands.json
python3 build_vp_clang_linux.py --config release --export-compile-commands
```

### Usage

```bash
# Run clang-tidy automation
python3 run_clang_tidy.py
```

### What It Does

1. **Reads** `compile_commands.json` (compilation database)
2. **Runs** clang-tidy on all source files
3. **Collects** suggested fixes in YAML format
4. **Filters** problematic fixes (VS2008 incompatible)
5. **Converts** C++11 constructs to C++03
6. **Resolves** overlapping replacements
7. **Validates** fixes before applying
8. **Applies** fixes to source files

### Output Files

```
clang-tidy-combined-results.txt       # Full analysis results
clang-tidy-combined-fixes.yaml        # Raw fixes (before filtering)
clang-tidy-combined-fixes.processed.yaml  # Filtered, safe fixes
```

### VS2008 Compatibility Features

**C++11 to C++03 Conversion:**
```cpp
// C++11 (clang-tidy suggests)
int* ptr = nullptr;

// C++03 (auto-converted)
int* ptr = NULL;
```

**Problematic Pattern Filtering:**
```cpp
// FILTERED: va_list can't be initialized in VS2008
va_list vl = {};  // ❌ Filtered out

// FILTERED: std::to_string not in VS2008
std::to_string(x);  // ❌ Filtered out

// FILTERED: Corruption detection
va_arg(vl = NULL, char*);  // ❌ Filtered (clang-tidy bug)
```

### Example Output

```
Found 156 C++ source files
Running clang-tidy...
  [====================] 156/156 files (100%)
  
Collected fixes: 1,785 suggestions
Filtering VS2008 incompatible patterns...
  Filtered: 30 problematic fixes
  Converted: 45 C++11 → C++03
  Resolved: 8 overlapping replacements
  
Safe fixes: 1,707 ready to apply
  
Apply fixes? [y/N]:
```

---

## Tool 2: Static Analysis (Clang Analyzer)

### What It Does

Runs deep static analysis to find bugs that normal compilation misses:
- Null pointer dereferences
- Memory leaks
- Use-after-free
- Uninitialized variables
- Dead code
- Logic errors

### Usage

```bash
# Run analysis (Debug build recommended for better warnings)
python3 build_vp_clang_linux.py --config debug --analyze
```

### What It Does

1. **Compiles** with `clang --analyze` (instead of normal compilation)
2. **Generates** .plist files (XML format with warnings)
3. **One plist** per source file

### Output

```
clang-build/Debug/CvGameCoreDLL_Expansion2/
├── CvCity.cpp.plist
├── CvPlayer.cpp.plist
├── CvUnit.cpp.plist
└── ... (156 plist files)
```

### Plist Format

```xml
<?xml version="1.0" encoding="UTF-8"?>
<plist version="1.0">
<dict>
 <key>files</key>
 <array>
  <string>CvCity.cpp</string>
 </array>
 <key>diagnostics</key>
 <array>
  <dict>
   <key>description</key>
   <string>Potential null pointer dereference</string>
   <key>category</key>
   <string>Logic error</string>
   <key>type</key>
   <string>Dereference of null pointer</string>
   <key>location</key>
   <dict>
    <key>line</key>
    <integer>1234</integer>
    ...
```

### Performance

- **Time:** ~180 seconds (slower than normal build)
- **Output:** ~152 warnings per build (typical)
- **Size:** Plist files ~500 KB total

---

## Tool 3: Plist Analysis

### What It Does

Aggregates and summarizes static analysis results from plist files.

### Usage

```bash
# After running --analyze, collate results
python3 collate_plist_results.py

# Generate summary report
python3 final_analysis_summary.py
```

### collate_plist_results.py

**Reads:** All .plist files in `clang-build/Debug/` and `clang-build/Release/`

**Generates:**
1. **Console summary** - High-level statistics
2. **static_analysis_results.json** - Detailed JSON report
3. **Critical warnings report** - High-priority issues

**Output Example:**

```
================================================================================
STATIC ANALYSIS SUMMARY
================================================================================

BUILD: Debug
Files analyzed: 156
Total warnings: 152

Warning Categories:
  Logic error              42
  Memory error             23
  Null pointer             18
  Uninitialized value      15
  Dead code                12
  ...

Top 10 Files by Warning Count:
  CvCity.cpp               12 warnings
  CvPlayer.cpp             10 warnings
  CvUnit.cpp                8 warnings
  ...

CRITICAL WARNINGS (High Priority):
  1. CvCity.cpp:1234 - Null pointer dereference
  2. CvPlayer.cpp:567 - Use after free
  ...
```

### final_analysis_summary.py

**Reads:** Build logs + plist files

**Generates:**
- Build performance metrics
- Plist file statistics
- Analysis timing
- Warning trends

**Output Example:**

```
================================================================================
FINAL ANALYSIS SUMMARY
LLVM 21.1.8 with --analyzer-output plist-multi-file
================================================================================

BUILD PERFORMANCE:
Metric                         Debug           Release        
------------------------------------------------------------
Analysis Time (seconds)        182.4           187.1          
Total Log Warnings             1101            1101           
Static Analysis Warnings       152             152            

PLIST FILE ANALYSIS:
Generated Plist Files          156             156            
Total Plist Size (KB)          487.2           489.1          

KEY FINDINGS:
1. LLVM 21.1.8 plist-multi-file option works successfully
2. Analysis completed for both Debug and Release builds
3. 152 static analysis warnings found in each build (consistent)
```

---

## Tool 4: Fix Filtering

### What It Does

Filters clang-tidy fix suggestions to ensure VS2008/C++03 compatibility.

### Usage

Normally called automatically by `run_clang_tidy.py`, but can be used standalone:

```bash
python3 filter_fixes.py clang-tidy-combined-fixes.yaml
```

### Filtering Rules

**1. C++11 to C++03 Conversion**
```cpp
// Before (C++11)
ptr = nullptr;

// After (C++03)
ptr = NULL;
```

**2. va_list Initialization Filtering**
```cpp
// Filtered (VS2008 doesn't support)
va_list vl = {};

// Allowed (correct VS2008 style)
va_list vl;
va_start(vl, format);
```

**3. std::to_string Filtering**
```cpp
// Filtered (not in VS2008)
std::to_string(value);

// Must use (VS2008 compatible)
char buf[32];
sprintf(buf, "%d", value);
```

**4. Corruption Detection**

Detects and filters clang-tidy bugs:
```cpp
// Corrupted (clang-tidy bug)
va_arg(vl = NULL, char*);
function())

// Should be
va_arg(vl, char*);
function()
```

### Output

```
clang-tidy-combined-fixes.filtered.yaml  # Filtered, safe fixes
```

---

## Integration with IDEs

### Visual Studio Code

**1. Install extensions:**
- C/C++ (Microsoft)
- clangd (LLVM)

**2. Configure `.vscode/settings.json`:**
```json
{
  "clangd.arguments": [
    "--compile-commands-dir=${workspaceFolder}",
    "--background-index",
    "--clang-tidy"
  ]
}
```

**3. Export compile_commands.json:**
```bash
python3 build_vp_clang_linux.py --config release --export-compile-commands
```

### CLion

**1. Open project**

**2. CLion auto-detects `compile_commands.json`**

**3. Enable Clang-Tidy:**
- Settings → Tools → Clang-Tidy
- Check "Enable Clang-Tidy"
- Configure checks (optional)

### Vim/Neovim

**1. Install coc.nvim or ALE**

**2. Configure clangd:**
```vim
let g:coc_global_extensions = ['coc-clangd']
```

**3. clangd auto-uses `compile_commands.json`**

---

## Troubleshooting

### Issue: "PyYAML not installed"

**Symptom:** `run_clang_tidy.py` crashes with ImportError

**Solution:**
```bash
pip3 install pyyaml
# Or: pip3 install -r requirements.txt
```

### Issue: "compile_commands.json not found"

**Symptom:** `run_clang_tidy.py` exits with error

**Solution:**
```bash
python3 build_vp_clang_linux.py --config release --export-compile-commands
```

### Issue: "No plist files found"

**Symptom:** `collate_plist_results.py` finds 0 files

**Solution:** Run static analysis first:
```bash
python3 build_vp_clang_linux.py --config debug --analyze
```

### Issue: Clang-tidy is slow

**Expected:** ~45 minutes for full analysis of 156 files

**Optimization:**
- Run on specific files only (edit run_clang_tidy.py)
- Use faster checks (remove expensive checks)
- Run on subset of files first

### Issue: Too many warnings

**Expected:** ~1,700+ suggestions is normal

**Action:**
- Review by category (most important first)
- Apply incrementally (test after each batch)
- Focus on critical issues first

---

## Best Practices

### When to Run Clang-Tidy

✅ **Do run:**
- Before major commits
- After large refactoring
- When fixing bugs (may reveal related issues)
- Periodically (monthly/quarterly)

❌ **Don't run:**
- On every build (too slow)
- Without VS2008 filtering (will break code)
- Without testing afterwards (verify fixes work)

### When to Run Static Analysis

✅ **Do run:**
- When investigating hard-to-find bugs
- Before releases
- After memory-related changes
- When seeing crashes in production

❌ **Don't run:**
- On every build (slow)
- Without reviewing results (generates lots of warnings)

### Applying Fixes

**Best practice workflow:**

1. **Backup first:**
   ```bash
   git commit -am "Before clang-tidy"
   ```

2. **Run analysis:**
   ```bash
   python3 run_clang_tidy.py
   ```

3. **Review fixes:**
   ```bash
   git diff  # Review what changed
   ```

4. **Test thoroughly:**
   ```bash
   python3 build_vp_clang_linux.py --config debug
   python3 build_vp_clang_linux.py --config release
   # Run game tests
   ```

5. **Commit if good:**
   ```bash
   git commit -am "Apply clang-tidy fixes"
   ```

6. **Revert if bad:**
   ```bash
   git reset --hard HEAD~1
   ```

---

## Technical Details

### Clang-Tidy Configuration

**Built-in configuration:**
```yaml
Checks: |
  readability-*,
  cppcoreguidelines-init-variables,
  modernize-use-bool-literals,
  -readability-magic-numbers,
  -readability-implicit-bool-conversion
```

**VS2008 incompatible checks disabled:**
- `modernize-*` (except bool-literals) - C++11+ features
- `cppcoreguidelines-pro-*` - Uses C++11+

### Static Analyzer Configuration

**Analysis mode:**
```bash
--analyze
--analyzer-output plist-multi-file
-Xclang -analyzer-checker=core,deadcode,nullability,security
```

**Checkers enabled:**
- `core` - Core checks (null dereference, divide by zero)
- `deadcode` - Dead code detection
- `nullability` - Null pointer analysis
- `security` - Security vulnerabilities

### Plist Format

Apple Property List (plist) XML format:
- Cross-platform
- Human-readable
- Machine-parseable
- Standard for clang analyzer

**Advantages:**
- Easy to parse with Python
- Can be viewed in Xcode (macOS)
- Structured data (not plain text)

---

## References

### Clang-Tidy Documentation
- https://clang.llvm.org/extra/clang-tidy/

### Clang Static Analyzer
- https://clang-analyzer.llvm.org/

### VS2008 Compatibility Guidelines
- See: `/workspace/Community-Patch-DLL-notes.txt`

---

**Last Updated:** January 2025  
**LLVM Version:** 21.1.8  
**Python:** 3.8+
