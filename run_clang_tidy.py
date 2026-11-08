#!/usr/bin/env python3
"""
Run clang-tidy over the DLL with the checks proven safe for VS2008 / C++03,
filter the fixes for VS2008, apply them, and re-check that the changed code
still compiles. Works on Linux and on Windows (same compile database, made by
build_vp_clang_linux.py --compile-commands-only, which compiles nothing).

Quick start (repository root):
    python3 run_clang_tidy.py --report-only --files CvDealAI.cpp   # look only
    python3 run_clang_tidy.py --files "CvDeal*.cpp"                # fix a few TUs
    python3 run_clang_tidy.py                                      # all TUs
  Windows: the same with "py" instead of "python3".

What it does:
 1. compile_commands.json: generated on first use (no compile, no build) by
    build_vp_clang_linux.py --compile-commands-only. Needs the VC9 + SDK 7.0A
    headers under Dependencies/ or --deps-dir / VP_DEPS_DIR.
 2. One clang-tidy process per translation unit, at most --jobs at a time
    (default min(6, CPUs/2): each clang-tidy holds a full TU AST in memory).
    Only the selected checks run ("-*," first, and --config replaces any
    .clang-tidy file found above the tree).
 3. Fixes are filtered for VS2008 (va_list / NAN / std::to_string / aggregate
    "= 0" / nullptr -> NULL), compiler fix-its are never applied, identical
    header fixes reported by several TUs are applied once, and overlapping
    fixes are skipped whole (picked up by the next pass, --passes).
 4. Files are changed byte-exactly: clang-tidy's offsets are byte offsets into
    the file as it is (CRLF and UTF-8), so nothing is converted to LF.
 5. Every TU that includes a changed file is re-checked with clang
    -fsyntax-only. If one fails, every changed file is restored.
    This is clang, not the VS2008 compiler: build with MSVC v90 before
    committing.

Reports go to .clang-tidy-outputs/ (diagnostics.txt, summary.json, per-TU logs
and fix files).
"""

import argparse
import concurrent.futures
import fnmatch
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
from collections import Counter, defaultdict
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
import apply_yaml_fixes as ayf  # noqa: E402  (needs PyYAML; exits with a message if missing)

IS_WINDOWS = os.name == "nt"
SOURCE_DIR = "CvGameCoreDLL_Expansion2"

# Proven checks (Documents/clang-tidy notes, section 2). Order is informational:
# all run in one pass; fixes that collide (e.g. isolate-declaration and
# init-variables on the same declaration) are applied over successive passes.
PROVEN_CHECKS = [
    "readability-isolate-declaration",
    "cppcoreguidelines-init-variables",
    "readability-inconsistent-declaration-parameter-name",
    "modernize-use-bool-literals",
    "readability-simplify-boolean-expr",  # review each change
    "readability-container-size-empty",
    "readability-string-compare",
    "readability-avoid-return-with-void-value",
    "readability-redundant-declaration",
    "readability-redundant-function-ptr-dereference",
    "readability-redundant-smartptr-get",
    "readability-redundant-string-cstr",
    "readability-redundant-string-init",
    "readability-static-accessed-through-instance",
]
MANUAL_REVIEW_CHECKS = {"readability-simplify-boolean-expr"}

# Planned, untested (notes section 6). Run with --checks to evaluate, --report-only first.
PLANNED_CHECKS = [
    "performance-faster-string-find",
    "performance-inefficient-algorithm",
    "performance-unnecessary-copy-initialization",
    "performance-inefficient-string-concatenation",
    "performance-inefficient-vector-operation",
    "performance-type-promotion-in-math-fn",
    "performance-for-range-copy",
    "performance-unnecessary-value-param",
    "performance-implicit-conversion-in-loop",
    "portability-simd-intrinsics",
    "portability-std-allocator-const",
]

# Never run (notes section 3): C++11 output, breaks the build, or rejected upstream.
BLACKLISTED_CHECKS = {
    "modernize-deprecated-headers",
    "modernize-use-auto",
    "modernize-redundant-void-arg",
    "readability-implicit-bool-conversion",
    "readability-math-missing-parentheses",
    "cppcoreguidelines-pro-type-member-init",
    "modernize-replace-auto-ptr",
    "modernize-loop-convert",
    "modernize-use-equals-delete",
    "performance-move-const-arg",
    "performance-move-constructor-init",
    "performance-no-automatic-move",
    "performance-noexcept-move-constructor",
}

# Checked against the changed source line (line text with the replacement in place).
VS2008_LINE_PATTERNS = [
    (re.compile(r"\bva_list\b[^;]*=\s*(NULL|nullptr|0|\{\})"), "va_list cannot be initialised (C2552)"),
    (re.compile(r"=\s*NAN\b"), "NAN is not defined by VS2008"),
    (re.compile(r"\bstd::to_string\b"), "std::to_string is not in VS2008"),
    (re.compile(r"=\s*\{\s*\}"), "empty brace initialiser is C++11"),
    (re.compile(r"\bConnections\s+\w+\s*=\s*0\b"), "aggregate initialised with 0"),
    (re.compile(r"\bVoteCommitmentList\s+\w+\s*=\s*0\b"), "aggregate initialised with 0"),
    (re.compile(r"\bIDInfoVector\s+\w+\s*=\s*0\b"), "aggregate initialised with 0"),
    (re.compile(r"\bLookupTable\s+\w+\s*=\s*0\b"), "aggregate initialised with 0"),
    (re.compile(r"\bTurnData\s+\w+\s*=\s*0\b"), "aggregate initialised with 0"),
    (re.compile(r"\bPlotStatePerTurn\s+\w+\s*=\s*0\b"), "aggregate initialised with 0"),
]
# Checked against the replacement text alone.
VS2008_TEXT_PATTERNS = [
    (re.compile(r"#\s*include\s*<(c?math(\.h)?)>"), "adds a math header"),
    (re.compile(r"\b(auto|constexpr|override|final|noexcept|decltype|static_assert)\b"), "C++11 keyword"),
]


def default_jobs():
    return max(1, min(6, (os.cpu_count() or 2) // 2))


def log(msg=""):
    print(msg, flush=True)


# --------------------------------------------------------------------------- tools

def tool_version(path):
    try:
        out = subprocess.run([path, "--version"], capture_output=True, text=True, timeout=60).stdout
    except (OSError, subprocess.SubprocessError):
        return None
    m = re.search(r"version (\d+[.\d]*)", out)
    return m.group(1) if m else "unknown"


def find_clang_tidy(explicit):
    exe = "clang-tidy.exe" if IS_WINDOWS else "clang-tidy"
    # An explicit choice wins as given.
    for chosen in (explicit, os.environ.get("CLANG_TIDY"),
                   os.path.join(os.environ["LLVM_PATH"], "bin", exe) if os.environ.get("LLVM_PATH") else None):
        if chosen:
            v = tool_version(chosen) if os.path.isfile(chosen) else None
            if not v:
                log("ERROR: clang-tidy %s does not exist or does not run." % chosen)
                sys.exit(1)
            return chosen, v
    # Otherwise the newest one found (several LLVM installs are common on Windows).
    candidates = []
    w = shutil.which("clang-tidy")
    if w:
        candidates.append(w)
    if IS_WINDOWS:
        for env in ("ProgramFiles", "ProgramW6432", "ProgramFiles(x86)"):
            if os.environ.get(env):
                candidates.append(os.path.join(os.environ[env], "LLVM", "bin", exe))
    else:
        versioned = []
        for d in os.environ.get("PATH", "").split(os.pathsep):
            try:
                for name in os.listdir(d):
                    m = re.fullmatch(r"clang-tidy-(\d+)", name)
                    if m:
                        versioned.append((int(m.group(1)), os.path.join(d, name)))
            except OSError:
                pass
        candidates += [p for _, p in sorted(versioned, reverse=True)]
    found = {}
    for c in candidates:
        if c and os.path.isfile(c) and ayf.norm_path(c) not in found:
            v = tool_version(c)
            if v:
                found[ayf.norm_path(c)] = (c, v)
    if found:
        def vkey(item):
            return tuple(int(x) for x in re.findall(r"\d+", item[1])[:3])
        return max(found.values(), key=vkey)
    log("ERROR: clang-tidy not found. Install LLVM (Windows: C:\\Program Files\\LLVM; Linux: "
        "apt install clang-tidy) or pass --clang-tidy / set CLANG_TIDY or LLVM_PATH.")
    sys.exit(1)


# --------------------------------------------------------------------------- compile database

def ensure_compile_db(args):
    db = Path(args.p).resolve() / "compile_commands.json"
    if db.exists() and not args.regenerate_db:
        return db
    if args.no_generate_db:
        log("ERROR: %s not found (and --no-generate-db given)." % db)
        sys.exit(1)
    cmd = [sys.executable, str(SCRIPT_DIR / "build_vp_clang_linux.py"), "--compile-commands-only",
           "--config", args.config, "--compile-commands-dir", str(db.parent)]
    if args.deps_dir:
        cmd += ["--deps-dir", args.deps_dir]
    log("Generating %s (no compile): %s" % (db, " ".join(cmd)))
    if subprocess.run(cmd, cwd=str(SCRIPT_DIR)).returncode != 0 or not db.exists():
        log("ERROR: could not generate the compile database.")
        sys.exit(1)
    return db


def entry_args(entry):
    if "arguments" in entry:
        return list(entry["arguments"])
    return shlex.split(entry["command"], posix=not IS_WINDOWS)


def entry_file(entry):
    f = Path(entry["file"])
    if not f.is_absolute():
        f = Path(entry.get("directory", ".")) / f
    return f.resolve()


def select_tus(db_path, patterns):
    with open(db_path, "r", encoding="utf-8") as fh:
        entries = json.load(fh)
    src_root = (SCRIPT_DIR / SOURCE_DIR).resolve()
    tus = {}
    for e in entries:
        f = entry_file(e)
        try:
            rel = f.relative_to(src_root)
        except ValueError:
            continue
        tus[ayf.norm_path(f)] = (f, rel.as_posix(), e)
    if not patterns:
        return list(tus.values()), list(tus.values())
    chosen = []
    for f, rel, e in tus.values():
        for pat in patterns:
            pat = pat.replace("\\", "/")
            if pat.startswith(SOURCE_DIR + "/"):
                pat = pat[len(SOURCE_DIR) + 1:]
            if fnmatch.fnmatch(rel.lower(), pat.lower()) or fnmatch.fnmatch(f.name.lower(), pat.lower()):
                chosen.append((f, rel, e))
                break
    if not chosen:
        log("ERROR: no translation unit in the compile database matches %s" % patterns)
        sys.exit(1)
    return chosen, list(tus.values())


# --------------------------------------------------------------------------- running

def run_parallel(func, items, jobs):
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [pool.submit(func, it) for it in items]
        done = 0
        for fut in concurrent.futures.as_completed(futures):
            results.append(fut.result())
            done += 1
            if done % 10 == 0 or done == len(items):
                log("  %d/%d" % (done, len(items)))
    return results


def tidy_one(tu, args, tidy, checks, out_dir, pass_no):
    f, rel, _ = tu
    stem = rel.replace("/", "__")
    fix_path = out_dir / "fixes" / ("pass%d" % pass_no) / (stem + ".yaml")
    log_path = out_dir / "logs" / ("pass%d" % pass_no) / (stem + ".txt")
    fix_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    if fix_path.exists():
        fix_path.unlink()
    config = "{Checks: '-*,%s', WarningsAsErrors: ''}" % ",".join(checks)
    cmd = [tidy, "--config=" + config, "--header-filter=" + args.header_filter,
           "--format-style=none", "--quiet", "-p", str(Path(args.p).resolve()),
           "--export-fixes=" + str(fix_path), str(f)]
    t0 = time.time()
    with open(log_path, "wb") as lf:
        rc = subprocess.run(cmd, stdout=lf, stderr=subprocess.STDOUT, cwd=str(SCRIPT_DIR)).returncode
    return {"tu": tu, "rc": rc, "fixes": fix_path if fix_path.exists() else None,
            "log": log_path, "secs": time.time() - t0}


def syntax_check_one(tu, out_dir, folder="verify"):
    f, rel, entry = tu
    cmd = entry_args(entry)
    # Drop the object output; -fsyntax-only parses and type-checks only.
    cleaned = []
    skip = False
    for a in cmd:
        if skip:
            skip = False
            continue
        if a == "-o":
            skip = True
            continue
        if a.startswith("-o") and len(a) > 2 and not a.startswith("-object"):
            continue
        if a in ("-MJ",):
            skip = True
            continue
        cleaned.append(a)
    cleaned.append("-fsyntax-only")
    log_path = out_dir / "logs" / folder / (rel.replace("/", "__") + ".txt")
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with open(log_path, "wb") as lf:
        rc = subprocess.run(cleaned, stdout=lf, stderr=subprocess.STDOUT,
                            cwd=entry.get("directory", str(SCRIPT_DIR))).returncode
    return {"tu": tu, "rc": rc, "log": log_path}


# --------------------------------------------------------------------------- filtering

class FileCache(object):
    def __init__(self):
        self.data = {}

    def get(self, path):
        k = ayf.norm_path(path)
        if k not in self.data:
            try:
                with open(path, "rb") as fh:
                    self.data[k] = fh.read()
            except OSError:
                self.data[k] = b""
        return self.data[k]

    def drop(self, paths):
        for p in paths:
            self.data.pop(ayf.norm_path(p), None)

    def line_col(self, path, offset):
        d = self.get(path)
        return d.count(b"\n", 0, offset) + 1, offset - (d.rfind(b"\n", 0, offset) + 1) + 1

    def preview(self, r):
        """The source line(s) touched by r with r applied, as text."""
        d = self.get(r.path)
        start = d.rfind(b"\n", 0, r.offset) + 1
        end = d.find(b"\n", r.end)
        end = len(d) if end < 0 else end
        new = d[start:r.offset] + r.text.encode("utf-8") + d[r.end:end]
        return new.decode("utf-8", "replace")


def vs2008_filter(diags, cache):
    """Returns (kept, filtered[(diag, reason)], converted_count). Mutates nullptr -> NULL."""
    kept, filtered, converted = [], [], 0
    for d in diags:
        if not d.replacements:
            kept.append(d)
            continue
        if d.check.startswith("clang-diagnostic"):
            filtered.append((d, "compiler fix-it (never applied)"))
            continue
        reason = None
        for r in d.replacements:
            if "nullptr" in r.text:
                r.text = re.sub(r"\bnullptr\b", "NULL", r.text)
                converted += 1
            for pat, why in VS2008_TEXT_PATTERNS:
                if pat.search(r.text):
                    reason = why
                    break
            if reason:
                break
            line = cache.preview(r)
            for pat, why in VS2008_LINE_PATTERNS:
                if pat.search(line):
                    reason = why
                    break
            if reason:
                break
        if reason:
            filtered.append((d, reason))
        else:
            kept.append(d)
    return kept, filtered, converted


# --------------------------------------------------------------------------- main

def parse_args():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__[__doc__.index("What it does"):])
    ap.add_argument("--files", nargs="+", metavar="PATTERN",
                    help="TUs to analyse: names or globs, e.g. CvDealAI.cpp 'CvDeal*.cpp' (default: all)")
    ap.add_argument("--checks", help="comma-separated checks to run instead of the proven list")
    ap.add_argument("--add-checks", help="comma-separated checks to run in addition to --checks/proven")
    ap.add_argument("--list-checks", action="store_true", help="print the check lists and exit")
    ap.add_argument("--report-only", action="store_true", help="analyse and report; change no file")
    ap.add_argument("--passes", type=int, default=2,
                    help="analyse/apply rounds; later rounds pick up fixes skipped as overlapping (default 2)")
    ap.add_argument("--no-verify", action="store_true", help="skip the clang -fsyntax-only re-check")
    ap.add_argument("--verify-selected-only", action="store_true",
                    help="after a header fix, re-check only the selected TUs instead of every TU in the database")
    ap.add_argument("--allow-errors", action="store_true",
                    help="apply fixes even if some TUs had compile errors (their AST is partial)")
    ap.add_argument("-j", "--jobs", type=int, default=default_jobs(),
                    help="concurrent clang-tidy/clang processes (default %d)" % default_jobs())
    ap.add_argument("--header-filter", default=r".*[/\\]%s[/\\][^/\\]*\.h$" % SOURCE_DIR,
                    help="headers whose diagnostics are reported/fixed (default: the DLL's own headers)")
    ap.add_argument("--clang-tidy", help="clang-tidy executable (default: CLANG_TIDY, LLVM_PATH, PATH, Program Files\\LLVM)")
    ap.add_argument("-p", default=str(SCRIPT_DIR), help="directory holding compile_commands.json (default: repo root)")
    ap.add_argument("--config", default="debug", choices=["debug", "release"],
                    help="configuration for a generated compile database (default debug)")
    ap.add_argument("--deps-dir", help="VC9/SDK Dependencies folder for database generation (else VP_DEPS_DIR or ./Dependencies)")
    ap.add_argument("--regenerate-db", action="store_true", help="regenerate compile_commands.json first")
    ap.add_argument("--no-generate-db", action="store_true", help="fail instead of generating a missing database")
    ap.add_argument("--out-dir", default=str(SCRIPT_DIR / ".clang-tidy-outputs"), help="report folder")
    return ap.parse_args()


def resolve_checks(args):
    checks = [c.strip() for c in (args.checks.split(",") if args.checks else PROVEN_CHECKS) if c.strip()]
    if args.add_checks:
        checks += [c.strip() for c in args.add_checks.split(",") if c.strip()]
    bad = [c for c in checks if c in BLACKLISTED_CHECKS]
    if bad:
        log("ERROR: blacklisted check(s) requested (see the notes, section 3): %s" % ", ".join(bad))
        sys.exit(1)
    # A glob (e.g. readability-*) runs with the blacklisted checks it covers switched off.
    excluded = sorted(b for b in BLACKLISTED_CHECKS
                      if any("*" in c and not c.startswith("-") and fnmatch.fnmatch(b, c) for c in checks))
    return checks + ["-" + b for b in excluded]


def main():
    args = parse_args()
    if args.list_checks:
        log("Proven (default):\n  " + "\n  ".join(PROVEN_CHECKS))
        log("Planned, untested (use --checks ... --report-only):\n  " + "\n  ".join(PLANNED_CHECKS))
        log("Blacklisted (refused):\n  " + "\n  ".join(sorted(BLACKLISTED_CHECKS)))
        return 0
    os.chdir(str(SCRIPT_DIR))
    checks = resolve_checks(args)
    tidy, tidy_ver = find_clang_tidy(args.clang_tidy)
    db = ensure_compile_db(args)
    tus, db_tus = select_tus(db, args.files)
    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    jobs = max(1, args.jobs)
    log("clang-tidy %s: %s" % (tidy_ver, tidy))
    log("database: %s; TUs: %d; jobs: %d; checks: %d; mode: %s"
        % (db, len(tus), jobs, len(checks), "report only" if args.report_only else "fix"))

    cache = FileCache()
    backup = {}
    summary = {"clang_tidy": tidy, "version": tidy_ver, "checks": checks, "tus": len(tus),
               "jobs": jobs, "passes": []}
    diag_lines = []
    all_tus = list(tus)
    exit_code = 0
    try:
        for pass_no in range(1, max(1, args.passes) + 1):
            log("\n== pass %d: clang-tidy on %d TU(s)" % (pass_no, len(tus)))
            t0 = time.time()
            results = run_parallel(lambda tu: tidy_one(tu, args, tidy, checks, out_dir, pass_no), tus, jobs)
            secs = time.time() - t0
            yamls = sorted(r["fixes"] for r in results if r["fixes"])
            diags, dups = ayf.load_diagnostics(yamls)
            errors = [d for d in diags if d.check == "clang-diagnostic-error"]
            error_tus = sorted({str(r["log"]) for r in results if r["rc"] != 0})
            kept, filtered, converted = vs2008_filter(diags, cache)
            accepted, conflicting = ayf.select_non_conflicting(kept)
            per_check = Counter(d.check for d in diags)
            accepted_ids = {id(d) for d in accepted}
            p = {"pass": pass_no, "seconds": round(secs, 1), "diagnostics": len(diags),
                 "duplicates_dropped": dups, "compile_errors": len(errors),
                 "fixable": sum(1 for d in kept if d.replacements), "filtered_vs2008": len(filtered),
                 "nullptr_converted": converted, "overlapping_skipped": len(conflicting),
                 "accepted": len(accepted), "per_check": dict(per_check.most_common())}
            for d in diags:
                if d.path:
                    ln, col = cache.line_col(d.path, d.offset)
                    tag = ""
                    if id(d) in accepted_ids:
                        tag = " [fix%s]" % (": review" if d.check in MANUAL_REVIEW_CHECKS else "")
                    diag_lines.append("pass%d %s:%d:%d: %s: %s%s"
                                      % (pass_no, d.path, ln, col, d.check, d.message, tag))
            for d, why in filtered:
                if d.path:
                    ln, _ = cache.line_col(d.path, d.offset)
                    diag_lines.append("pass%d %s:%d: %s: FILTERED (%s)" % (pass_no, d.path, ln, d.check, why))
            log("  %.0f s; diagnostics %d (%d duplicate header reports dropped); fixes: %d accepted, "
                "%d filtered for VS2008, %d overlapping (next pass); nullptr->NULL %d"
                % (secs, len(diags), dups, len(accepted), len(filtered), len(conflicting), converted))
            for check, n in per_check.most_common():
                log("    %5d %s" % (n, check))
            if errors or error_tus:
                log("  WARNING: %d compile error(s); clang-tidy exited non-zero for %d TU(s). Logs:"
                    % (len(errors), len(error_tus)))
                for e in error_tus[:10]:
                    log("    " + e)
            summary["passes"].append(p)

            if args.report_only or not accepted:
                break
            if (errors or error_tus) and not args.allow_errors:
                log("  NOT applying: fix the compile errors first (or --allow-errors).")
                exit_code = 2
                break
            changed = ayf.apply_diagnostics(accepted, backup=backup)
            cache.drop(changed.keys())
            p["files_changed"] = len(changed)
            log("  applied %d fixes to %d file(s)" % (len(accepted), len(changed)))

            changed_headers = any(not k.endswith(".cpp") for k in changed)
            scope = all_tus if (args.verify_selected_only or not changed_headers) else db_tus
            affected = [tu for tu in scope if changed_headers or ayf.norm_path(tu[0]) in changed]
            if not args.no_verify and affected:
                log("  verify: clang -fsyntax-only on %d TU(s)" % len(affected))
                vres = run_parallel(lambda tu: syntax_check_one(tu, out_dir), affected, jobs)
                bad = [v for v in vres if v["rc"] != 0]
                p["verify_failed"] = len(bad)
                if bad:
                    log("  VERIFY FAILED in %d TU(s); restoring all %d changed file(s). Logs:"
                        % (len(bad), len(backup)))
                    for v in bad[:10]:
                        log("    " + str(v["log"]))
                    restore(backup)
                    # Tell fix-caused failures from ones the unchanged tree already has.
                    again = run_parallel(lambda v: syntax_check_one(v["tu"], out_dir, "verify-unchanged"), bad, jobs)
                    pre = sorted(a["tu"][1] for a in again if a["rc"] != 0)
                    if pre:
                        log("  %d of these also fail on the unchanged files (not caused by the fixes): %s"
                            % (len(pre), ", ".join(pre[:10])))
                        log("  Fix the build first, or use --no-verify.")
                    p["verify_preexisting"] = len(pre)
                    exit_code = 3
                    break
            if not conflicting:
                break
            # Next pass: only the TUs that can still have the skipped fixes.
            if changed_headers or any(not ayf.norm_path(d.path).endswith(".cpp") for d in conflicting if d.path):
                tus = all_tus
            else:
                want = {ayf.norm_path(d.path) for d in conflicting if d.path}
                tus = [tu for tu in all_tus if ayf.norm_path(tu[0]) in want]
    except KeyboardInterrupt:
        log("\nInterrupted: restoring %d changed file(s)." % len(backup))
        restore(backup)
        return 130
    except Exception:
        if backup:
            log("\nError: restoring %d changed file(s)." % len(backup))
            restore(backup)
        raise

    summary["files_changed"] = 0 if exit_code == 3 else len(backup)
    with open(out_dir / "summary.json", "w", encoding="utf-8") as fh:
        json.dump(summary, fh, indent=2)
    with open(out_dir / "diagnostics.txt", "w", encoding="utf-8") as fh:
        fh.write("\n".join(diag_lines) + "\n")
    log("\nReports: %s (diagnostics.txt, summary.json, logs/, fixes/)" % out_dir)
    if backup and exit_code == 0:
        log("Changed %d file(s). Review with git diff, then build with MSVC v90 (VS2008) before committing."
            % len(backup))
        if any(d in MANUAL_REVIEW_CHECKS for d in checks):
            log("readability-simplify-boolean-expr fixes are marked [fix: review] in diagnostics.txt.")
    return exit_code


def restore(backup):
    for path, data in backup.items():
        with open(path, "wb") as fh:
            fh.write(data)


if __name__ == "__main__":
    sys.exit(main())
