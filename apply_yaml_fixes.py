#!/usr/bin/env python3
"""
Apply clang-tidy --export-fixes YAML files to the source tree, byte-exactly.

Used by run_clang_tidy.py; can also be run on its own:

    python3 apply_yaml_fixes.py fixes1.yaml [fixes2.yaml ...] [--dry-run] [-v]

How it works:
- clang-tidy reports Offset/Length as BYTE offsets into the file exactly as it
  is on disk (CRLF and multi-byte UTF-8 included; checked with clang-tidy 18
  and 23 on a CRLF + UTF-8 file). So files are read and written as bytes and
  never re-encoded or converted to LF.
- Replacement text uses LF. In a CRLF file every inserted LF is written as
  CRLF, so the file keeps one line-ending style.
- The same diagnostic from a header is reported once per translation unit that
  includes it; identical diagnostics are applied once.
- A diagnostic is applied whole or not at all. If any of its replacements
  overlaps a replacement already accepted for the same file, the whole
  diagnostic is skipped (applying half a fix is what corrupts code). Run
  clang-tidy again to pick up skipped fixes.
- Replacements are applied from the end of each file to the start, so earlier
  offsets stay valid.
"""

import argparse
import os
import sys
from collections import defaultdict
from pathlib import Path

try:
    import yaml
except ImportError:
    print("Error: PyYAML is required. Install it with: pip install pyyaml")
    sys.exit(1)

try:
    _YamlLoader = yaml.CSafeLoader
except AttributeError:
    _YamlLoader = yaml.SafeLoader


def norm_path(path):
    """Canonical key for a file path (case-insensitive on Windows)."""
    return os.path.normcase(os.path.abspath(path))


class Replacement(object):
    __slots__ = ("path", "offset", "length", "text")

    def __init__(self, path, offset, length, text):
        self.path = path
        self.offset = int(offset)
        self.length = int(length)
        self.text = text if text is not None else ""

    @property
    def end(self):
        return self.offset + self.length

    def key(self):
        return (norm_path(self.path), self.offset, self.length, self.text)

    def overlaps(self, other):
        """True if the two replacements touch the same bytes.

        Two insertions (length 0) at the same offset also conflict: their order
        would be arbitrary."""
        if self.offset == other.offset:
            return True
        return self.offset < other.end and other.offset < self.end


class Diagnostic(object):
    __slots__ = ("check", "message", "path", "offset", "replacements")

    def __init__(self, check, message, path, offset, replacements):
        self.check = check
        self.message = message
        self.path = path
        self.offset = offset
        self.replacements = replacements

    def key(self):
        return (self.check, tuple(sorted(r.key() for r in self.replacements)))


def load_diagnostics(yaml_paths):
    """Read clang-tidy fix files. Returns (diagnostics, duplicates_dropped).

    Diagnostics without replacements are kept (they are findings to report)."""
    seen = set()
    result = []
    dups = 0
    for yaml_path in yaml_paths:
        with open(yaml_path, "rb") as f:
            data = yaml.load(f, Loader=_YamlLoader)
        if not data:
            continue
        for diag in data.get("Diagnostics") or []:
            msg = diag.get("DiagnosticMessage") or {}
            repls = []
            for r in msg.get("Replacements") or []:
                if not r.get("FilePath"):
                    continue
                repls.append(Replacement(r["FilePath"], r.get("Offset", 0),
                                         r.get("Length", 0), r.get("ReplacementText", "")))
            d = Diagnostic(diag.get("DiagnosticName", "unknown"), msg.get("Message", ""),
                           msg.get("FilePath", ""), int(msg.get("FileOffset", 0) or 0), repls)
            k = (d.check, norm_path(d.path) if d.path else "", d.offset, d.message, d.key()[1])
            if k in seen:
                dups += 1
                continue
            seen.add(k)
            result.append(d)
    return result, dups


def select_non_conflicting(diagnostics):
    """Accept diagnostics in file/offset order; skip any that overlap one already
    accepted. Returns (accepted, conflicting)."""
    accepted = []
    conflicting = []
    taken = defaultdict(list)  # norm path -> accepted replacements
    fixable = [d for d in diagnostics if d.replacements]
    fixable.sort(key=lambda d: (norm_path(d.path or d.replacements[0].path), d.offset))
    for d in fixable:
        clash = False
        for r in d.replacements:
            for t in taken[norm_path(r.path)]:
                if r.overlaps(t):
                    clash = True
                    break
            if clash:
                break
        # Replacements inside one diagnostic must not overlap each other either.
        if not clash:
            rs = sorted(d.replacements, key=lambda r: (norm_path(r.path), r.offset))
            for a, b in zip(rs, rs[1:]):
                if norm_path(a.path) == norm_path(b.path) and a.overlaps(b):
                    clash = True
                    break
        if clash:
            conflicting.append(d)
            continue
        accepted.append(d)
        for r in d.replacements:
            taken[norm_path(r.path)].append(r)
    return accepted, conflicting


def apply_to_bytes(data, replacements):
    """Apply replacements (all for one file, non-overlapping) to a bytes object."""
    crlf = b"\r\n" in data
    for r in sorted(replacements, key=lambda r: r.offset, reverse=True):
        if r.offset < 0 or r.end > len(data):
            raise ValueError("replacement at %d+%d is outside the file (%d bytes)"
                             % (r.offset, r.length, len(data)))
        text = r.text.encode("utf-8")
        if crlf:
            text = text.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
        data = data[:r.offset] + text + data[r.end:]
    return data


def apply_diagnostics(diagnostics, dry_run=False, verbose=False, backup=None):
    """Write the accepted diagnostics' replacements to disk.

    backup: optional dict {norm path: original bytes}; filled before a file is
    written so the caller can restore it. Returns {norm path: replacements applied}."""
    by_file = defaultdict(list)
    for d in diagnostics:
        for r in d.replacements:
            by_file[norm_path(r.path)].append(r)
    applied = {}
    for path, repls in sorted(by_file.items()):
        with open(path, "rb") as f:
            original = f.read()
        new = apply_to_bytes(original, repls)
        if new == original:
            continue
        applied[path] = len(repls)
        if verbose or dry_run:
            print("  %s %s (%d replacements)" % ("[dry-run]" if dry_run else "modified",
                                                 path, len(repls)))
        if dry_run:
            continue
        if backup is not None and path not in backup:
            backup[path] = original
        with open(path, "wb") as f:
            f.write(new)
    return applied


def main():
    parser = argparse.ArgumentParser(description="Apply clang-tidy fix YAML files byte-exactly.")
    parser.add_argument("yaml_files", nargs="+", type=Path)
    parser.add_argument("--dry-run", action="store_true", help="report, do not write")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    missing = [p for p in args.yaml_files if not p.exists()]
    if missing:
        print("YAML file(s) not found: %s" % ", ".join(map(str, missing)))
        return 1
    diags, dups = load_diagnostics(args.yaml_files)
    accepted, conflicting = select_non_conflicting(diags)
    applied = apply_diagnostics(accepted, dry_run=args.dry_run, verbose=args.verbose)
    print("diagnostics: %d (%d duplicates dropped), fixes accepted: %d, skipped as overlapping: %d"
          % (len(diags), dups, len(accepted), len(conflicting)))
    print("files %s: %d" % ("that would change" if args.dry_run else "modified", len(applied)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
