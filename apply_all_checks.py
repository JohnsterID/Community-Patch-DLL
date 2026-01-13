#!/usr/bin/env python3
"""
Apply ALL safe clang-tidy checks at once using CRLF workaround
Much faster than one-at-a-time approach!
"""
import subprocess
import sys
from pathlib import Path
import shutil
import time

CLANG_TIDY = "/tmp/LLVM-21.1.8-Linux-X64/bin/clang-tidy"
CLANG_APPLY = "/tmp/LLVM-21.1.8-Linux-X64/bin/clang-apply-replacements"
SOURCE_DIR = Path("CvGameCoreDLL_Expansion2")

# All 13 proven safe checks (excluding readability-string-compare which is too slow)
ALL_CHECKS = [
    "readability-isolate-declaration",
    "modernize-use-bool-literals",
    "readability-container-size-empty",
    "readability-inconsistent-declaration-parameter-name",
    "cppcoreguidelines-init-variables",
    "readability-avoid-return-with-void-value",
    "readability-redundant-declaration",
    "readability-redundant-function-ptr-dereference",
    "readability-redundant-smartptr-get",
    "readability-redundant-string-cstr",
    "readability-redundant-string-init",
    "readability-static-accessed-through-instance",
    "readability-simplify-boolean-expr",
]

def print_banner(text):
    print("\n" + "=" * 80)
    print(text)
    print("=" * 80 + "\n")

def backup_files():
    """Backup all source files"""
    print("📦 Creating backup...")
    backup_dir = Path(".all-checks-backup")
    if backup_dir.exists():
        shutil.rmtree(backup_dir)
    backup_dir.mkdir()
    
    for cpp_file in SOURCE_DIR.glob("*.cpp"):
        shutil.copy(cpp_file, backup_dir / cpp_file.name)
    
    print(f"  ✓ Backed up {len(list(backup_dir.glob('*.cpp')))} files\n")
    return backup_dir

def convert_to_lf():
    """Convert all source files to LF"""
    print("🔄 Converting CRLF → LF...")
    crlf_files = []
    
    for cpp_file in SOURCE_DIR.glob("*.cpp"):
        content = cpp_file.read_bytes()
        if b'\r\n' in content:
            crlf_files.append(cpp_file)
            content_lf = content.decode('utf-8').replace('\r\n', '\n').encode('utf-8')
            cpp_file.write_bytes(content_lf)
    
    print(f"  ✓ Converted {len(crlf_files)} files\n")
    return crlf_files

def convert_to_crlf(crlf_files):
    """Convert source files back to CRLF"""
    print("🔄 Converting LF → CRLF...")
    
    for cpp_file in crlf_files:
        if not cpp_file.exists():
            continue
        content = cpp_file.read_bytes()
        content_crlf = content.decode('utf-8').replace('\n', '\r\n').encode('utf-8')
        cpp_file.write_bytes(content_crlf)
    
    print(f"  ✓ Converted {len(crlf_files)} files back\n")

def run_clang_tidy():
    """Run clang-tidy with ALL checks at once"""
    print_banner("STEP 1: Running clang-tidy with ALL 13 checks")
    
    checks_str = ",".join(ALL_CHECKS)
    cpp_files = list(SOURCE_DIR.glob("*.cpp"))
    
    print(f"Checks: {len(ALL_CHECKS)}")
    print(f"Files: {len(cpp_files)}")
    print()
    
    print("Running clang-tidy... (this may take 10-15 minutes)")
    start = time.time()
    
    result = subprocess.run([
        CLANG_TIDY,
        f"--checks=-*,{checks_str}",
        "--export-fixes=all-fixes.yaml",
        "-p=.",
    ] + [str(f) for f in cpp_files],
    capture_output=True,
    text=True,
    timeout=1200  # 20 minute timeout
    )
    
    duration = time.time() - start
    
    if not Path("all-fixes.yaml").exists():
        print(f"  ℹ️  No fixes generated ({duration:.1f}s)")
        return False
    
    # Count diagnostics
    with open("all-fixes.yaml") as f:
        content = f.read()
        if "Replacements:" not in content or "Replacements: []" in content:
            print(f"  ℹ️  No fixes needed ({duration:.1f}s)")
            Path("all-fixes.yaml").unlink()
            return False
    
    print(f"  ✓ Fixes generated: all-fixes.yaml ({duration:.1f}s)")
    print(f"  ✓ YAML size: {Path('all-fixes.yaml').stat().st_size / 1024:.1f} KB\n")
    return True

def apply_fixes():
    """Apply all generated fixes"""
    print_banner("STEP 2: Applying ALL fixes at once")
    
    print("Applying fixes with clang-apply-replacements...")
    start = time.time()
    
    result = subprocess.run([
        CLANG_APPLY,
        ".",
    ], capture_output=True, text=True, timeout=120)
    
    duration = time.time() - start
    
    if result.returncode != 0:
        print(f"  ❌ Failed to apply fixes ({duration:.1f}s)")
        print(result.stderr[:500] if result.stderr else "")
        return False
    
    print(f"  ✓ Fixes applied successfully ({duration:.1f}s)\n")
    return True

def check_diff():
    """Check what changed"""
    print("📊 Checking changes...")
    
    result = subprocess.run(
        ["git", "diff", "--stat", "CvGameCoreDLL_Expansion2/"],
        capture_output=True,
        text=True
    )
    
    if result.stdout.strip():
        print(result.stdout)
        
        # Count files changed
        lines = result.stdout.strip().split('\n')
        if len(lines) > 1:
            summary = lines[-1]
            print(f"\n📈 Summary: {summary}\n")
        return True
    
    print("  ℹ️  No changes detected\n")
    return False

def build():
    """Build the DLL"""
    print_banner("STEP 3: Building DLL to validate changes")
    
    print("🔨 Building...")
    start = time.time()
    
    result = subprocess.run(
        ["python3", "build_vp_clang_linux.py"],
        capture_output=True,
        text=True,
        timeout=300
    )
    
    duration = time.time() - start
    
    if result.returncode != 0:
        print(f"  ❌ Build failed after {duration:.1f}s\n")
        print("--- Last 100 lines of output ---")
        print(result.stdout[-5000:] if result.stdout else "")
        print(result.stderr[-5000:] if result.stderr else "")
        return False
    
    print(f"  ✓ Build successful ({duration:.1f}s)\n")
    
    # Show DLL info
    dll_path = Path("clang-output/Release/CvGameCore_Expansion2.dll")
    if dll_path.exists():
        size_mb = dll_path.stat().st_size / (1024 * 1024)
        print(f"  ✓ DLL generated: {size_mb:.1f} MB\n")
    
    return True

def show_sample_changes():
    """Show sample of what changed"""
    print("📝 Sample changes (first file):")
    print("-" * 80)
    
    result = subprocess.run(
        ["git", "diff", "-U3", "CvGameCoreDLL_Expansion2/"],
        capture_output=True,
        text=True
    )
    
    if result.stdout:
        lines = result.stdout.split('\n')
        # Show first 50 lines
        for line in lines[:50]:
            print(line)
        if len(lines) > 50:
            print(f"\n... ({len(lines) - 50} more lines)")
    print()

def main():
    print_banner("🚀 APPLY ALL CLANG-TIDY CHECKS AT ONCE")
    
    print(f"This will run {len(ALL_CHECKS)} checks simultaneously:")
    for i, check in enumerate(ALL_CHECKS, 1):
        print(f"  {i:2}. {check}")
    print()
    print("Excluded: readability-string-compare (too slow)")
    print()
    
    print("Estimated time: 15-20 minutes")
    print("(vs 2+ hours if run one-at-a-time)")
    print()
    
    input("Press Enter to continue (or Ctrl+C to cancel)...")
    
    start_time = time.time()
    
    try:
        # Backup
        backup_dir = backup_files()
        
        # Convert to LF
        crlf_files = convert_to_lf()
        
        # Run clang-tidy with ALL checks
        has_fixes = run_clang_tidy()
        
        if not has_fixes:
            print_banner("✅ RESULT: No fixes needed!")
            print("All code is already compliant with these checks.\n")
            return 0
        
        # Apply all fixes
        if not apply_fixes():
            print_banner("❌ RESULT: Failed to apply fixes")
            return 1
        
        # Convert back to CRLF
        convert_to_crlf(crlf_files)
        
        # Check diff
        has_changes = check_diff()
        
        if not has_changes:
            print_banner("❌ RESULT: Fixes applied but no changes detected")
            return 1
        
        # Show sample
        show_sample_changes()
        
        # Build
        if not build():
            print_banner("❌ RESULT: Build failed!")
            print("Restoring backup...")
            for backup in backup_dir.glob("*.cpp"):
                shutil.copy(backup, SOURCE_DIR / backup.name)
            print("✓ Backup restored\n")
            return 1
        
        # Success!
        total_time = time.time() - start_time
        
        print_banner("🎉 SUCCESS! All checks applied and build passed!")
        
        print(f"Total time: {total_time / 60:.1f} minutes")
        print()
        print("Next steps:")
        print("  1. Review changes: git diff")
        print("  2. If satisfied: git add -A && git commit")
        print("  3. If not: git checkout -- CvGameCoreDLL_Expansion2/")
        print()
        print(f"Backup available at: {backup_dir}/")
        print()
        
        return 0
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
        return 130
    
    except Exception as e:
        print(f"\n\n❌ Error: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())
