#!/usr/bin/env python3
"""
Comprehensive test: Apply all safe clang-tidy checks with CRLF workaround
Build and validate after each check
"""
import subprocess
import sys
from pathlib import Path
import shutil
import time

CLANG_TIDY = "/tmp/LLVM-21.1.8-Linux-X64/bin/clang-tidy"
CLANG_APPLY = "/tmp/LLVM-21.1.8-Linux-X64/bin/clang-apply-replacements"
SOURCE_DIR = Path("CvGameCoreDLL_Expansion2")

# Safe checks in order of priority
CHECKS = [
    ("modernize-use-bool-literals", "Use true/false instead of 0/1 for booleans"),
    ("readability-isolate-declaration", "Split multiple variable declarations"),
    ("readability-container-size-empty", "Use .empty() instead of .size() == 0"),
    ("readability-inconsistent-declaration-parameter-name", "Fix parameter name mismatches"),
    ("readability-redundant-string-cstr", "Remove unnecessary .c_str() calls"),
    ("readability-string-compare", "Simplify string comparisons"),
]

class CheckTester:
    def __init__(self):
        self.results = []
        self.crlf_files = []
        
    def backup_all(self):
        """Backup all source files"""
        print("📦 Creating backups...")
        backup_dir = Path(".test-backups")
        backup_dir.mkdir(exist_ok=True)
        
        for cpp_file in SOURCE_DIR.glob("*.cpp"):
            shutil.copy(cpp_file, backup_dir / cpp_file.name)
        
        print(f"  ✓ Backed up {len(list(backup_dir.glob('*.cpp')))} files\n")
        return backup_dir
    
    def restore_all(self, backup_dir):
        """Restore all source files from backup"""
        print("♻️  Restoring from backup...")
        for backup in backup_dir.glob("*.cpp"):
            shutil.copy(backup, SOURCE_DIR / backup.name)
        print(f"  ✓ Restored {len(list(backup_dir.glob('*.cpp')))} files\n")
    
    def convert_to_lf(self):
        """Convert all source files to LF"""
        print("🔄 Converting CRLF → LF...")
        self.crlf_files = []
        
        for cpp_file in SOURCE_DIR.glob("*.cpp"):
            content = cpp_file.read_bytes()
            if b'\r\n' in content:
                self.crlf_files.append(cpp_file)
                content_lf = content.decode('utf-8').replace('\r\n', '\n').encode('utf-8')
                cpp_file.write_bytes(content_lf)
        
        print(f"  ✓ Converted {len(self.crlf_files)} files\n")
    
    def convert_to_crlf(self):
        """Convert source files back to CRLF"""
        print("🔄 Converting LF → CRLF...")
        
        for cpp_file in self.crlf_files:
            content = cpp_file.read_bytes()
            content_crlf = content.decode('utf-8').replace('\n', '\r\n').encode('utf-8')
            cpp_file.write_bytes(content_crlf)
        
        print(f"  ✓ Converted {len(self.crlf_files)} files back\n")
    
    def run_clang_tidy(self, check_name):
        """Run clang-tidy for a specific check"""
        print(f"🔍 Running clang-tidy for {check_name}...")
        
        cpp_files = list(SOURCE_DIR.glob("*.cpp"))
        yaml_file = f"fixes-{check_name}.yaml"
        
        # Run clang-tidy
        result = subprocess.run([
            CLANG_TIDY,
            f"--checks=-*,{check_name}",
            f"--export-fixes={yaml_file}",
            "-p=.",
        ] + [str(f) for f in cpp_files],
        capture_output=True,
        text=True,
        timeout=600
        )
        
        if not Path(yaml_file).exists():
            print(f"  ℹ️  No fixes needed")
            return False
        
        # Count fixes
        with open(yaml_file) as f:
            content = f.read()
            if "Replacements:" not in content:
                print(f"  ℹ️  No fixes generated")
                Path(yaml_file).unlink()
                return False
        
        print(f"  ✓ Fixes generated: {yaml_file}")
        return True
    
    def apply_fixes(self, check_name):
        """Apply generated fixes"""
        print(f"🔧 Applying fixes...")
        
        result = subprocess.run([
            CLANG_APPLY,
            ".",
        ], capture_output=True, text=True, timeout=60)
        
        if result.returncode != 0:
            print(f"  ❌ Failed to apply fixes")
            print(result.stderr[:500])
            return False
        
        print(f"  ✓ Fixes applied")
        return True
    
    def check_diff(self):
        """Check what changed"""
        result = subprocess.run(
            ["git", "diff", "--stat", "CvGameCoreDLL_Expansion2/"],
            capture_output=True,
            text=True
        )
        
        if result.stdout.strip():
            print(f"\n📊 Changes:")
            print(result.stdout)
            return True
        
        print(f"  ℹ️  No changes detected")
        return False
    
    def build(self):
        """Build the DLL"""
        print(f"🔨 Building DLL...")
        start = time.time()
        
        result = subprocess.run(
            ["python3", "build_vp_clang_linux.py"],
            capture_output=True,
            text=True,
            timeout=300
        )
        
        duration = time.time() - start
        
        if result.returncode != 0:
            print(f"  ❌ Build failed after {duration:.1f}s")
            # Show last 50 lines of error
            print("\n--- Build errors ---")
            print(result.stdout[-2000:] if result.stdout else "")
            print(result.stderr[-2000:] if result.stderr else "")
            return False
        
        print(f"  ✓ Build successful ({duration:.1f}s)")
        return True
    
    def test_check(self, check_name, description):
        """Test a single check end-to-end"""
        print("=" * 70)
        print(f"Testing: {check_name}")
        print(f"Description: {description}")
        print("=" * 70)
        print()
        
        start_time = time.time()
        result = {
            'check': check_name,
            'description': description,
            'success': False,
            'has_fixes': False,
            'build_ok': False,
            'duration': 0,
            'error': None
        }
        
        try:
            # Run clang-tidy
            has_fixes = self.run_clang_tidy(check_name)
            result['has_fixes'] = has_fixes
            
            if not has_fixes:
                result['success'] = True
                result['build_ok'] = True  # No changes, build should still work
                print(f"✓ Check complete (no fixes needed)\n")
                return result
            
            # Apply fixes
            if not self.apply_fixes(check_name):
                result['error'] = "Failed to apply fixes"
                return result
            
            # Convert back to CRLF
            self.convert_to_crlf()
            
            # Check diff
            has_changes = self.check_diff()
            
            if not has_changes:
                result['error'] = "Fixes applied but no changes detected"
                return result
            
            # Build
            build_ok = self.build()
            result['build_ok'] = build_ok
            
            if not build_ok:
                result['error'] = "Build failed after applying fixes"
                return result
            
            result['success'] = True
            print(f"\n✅ SUCCESS: {check_name}\n")
            
        except Exception as e:
            result['error'] = str(e)
            print(f"\n❌ ERROR: {e}\n")
        
        finally:
            result['duration'] = time.time() - start_time
            
        return result
    
    def print_summary(self):
        """Print summary of all tests"""
        print("\n" + "=" * 70)
        print("COMPREHENSIVE TEST SUMMARY")
        print("=" * 70)
        print()
        
        successful = [r for r in self.results if r['success']]
        failed = [r for r in self.results if not r['success']]
        
        print(f"Total checks tested: {len(self.results)}")
        print(f"✅ Successful: {len(successful)}")
        print(f"❌ Failed: {len(failed)}")
        print()
        
        if successful:
            print("✅ SUCCESSFUL CHECKS:")
            print("-" * 70)
            for r in successful:
                status = "No fixes needed" if not r['has_fixes'] else f"Build OK ({r['duration']:.1f}s)"
                print(f"  ✓ {r['check']}")
                print(f"    {r['description']}")
                print(f"    Status: {status}")
                print()
        
        if failed:
            print("❌ FAILED CHECKS:")
            print("-" * 70)
            for r in failed:
                print(f"  ✗ {r['check']}")
                print(f"    {r['description']}")
                print(f"    Error: {r['error']}")
                print()
        
        print("=" * 70)
        
        if len(successful) == len(self.results):
            print("🎉 ALL CHECKS PASSED! 🎉")
        else:
            print(f"⚠️  {len(failed)} check(s) failed")
        
        print("=" * 70)
        
    def run_all(self):
        """Run all checks sequentially"""
        print("=" * 70)
        print("CLANG-TIDY COMPREHENSIVE TEST")
        print("Testing all safe checks with build validation")
        print("=" * 70)
        print()
        
        # Backup
        backup_dir = self.backup_all()
        
        try:
            # Convert to LF once
            self.convert_to_lf()
            
            # Test each check
            for check_name, description in CHECKS:
                result = self.test_check(check_name, description)
                self.results.append(result)
                
                # If build failed, restore and continue
                if not result['build_ok'] and result['has_fixes']:
                    print("⚠️  Restoring backup before next check...\n")
                    self.restore_all(backup_dir)
                    self.convert_to_lf()
                
                # Clean up YAML files
                for yaml in Path(".").glob("fixes-*.yaml"):
                    yaml.unlink()
                for yaml in Path(".").glob("*.fixes.yaml"):
                    yaml.unlink()
            
            # Print summary
            self.print_summary()
            
        finally:
            # Restore everything
            print("\n♻️  Restoring original state...")
            self.restore_all(backup_dir)
            
            # Clean up
            shutil.rmtree(backup_dir)
            print("  ✓ Cleanup complete\n")

if __name__ == "__main__":
    tester = CheckTester()
    tester.run_all()
