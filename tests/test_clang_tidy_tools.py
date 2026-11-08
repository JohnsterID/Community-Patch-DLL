#!/usr/bin/env python3
"""Unit tests for apply_yaml_fixes.py and run_clang_tidy.py's VS2008 filter.

No clang-tidy needed. Run from the repository root on Linux or Windows:
    python3 tests/test_clang_tidy_tools.py      (Windows: py tests\test_clang_tidy_tools.py)
"""
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

import yaml  # noqa: E402

import apply_yaml_fixes as ayf  # noqa: E402
import run_clang_tidy as rct  # noqa: E402


def fixes_yaml(path, diags):
    """diags: list of (check, file, [(offset, length, text), ...])."""
    data = {"MainSourceFile": str(path), "Diagnostics": []}
    for check, f, repls in diags:
        data["Diagnostics"].append({
            "DiagnosticName": check,
            "DiagnosticMessage": {
                "Message": "m", "FilePath": str(f), "FileOffset": repls[0][0] if repls else 0,
                "Replacements": [{"FilePath": str(f), "Offset": o, "Length": n, "ReplacementText": t}
                                 for o, n, t in repls]}})
    with open(path, "w") as fh:
        yaml.safe_dump(data, fh)
    return path


class ApplyTests(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp())

    def tearDown(self):
        shutil.rmtree(str(self.dir))

    def src(self, data):
        p = self.dir / "a.cpp"
        p.write_bytes(data)
        return p

    def run_fixes(self, *diag_lists):
        yamls = [fixes_yaml(self.dir / ("f%d.yaml" % i), d) for i, d in enumerate(diag_lists)]
        diags, dups = ayf.load_diagnostics(yamls)
        accepted, conflicting = ayf.select_non_conflicting(diags)
        ayf.apply_diagnostics(accepted)
        return dups, accepted, conflicting

    def test_crlf_utf8_byte_offsets(self):
        # Offsets as clang-tidy reports them: raw bytes, \r and the 2-byte (c) counted.
        data = b"// \xc2\xa9\r\nint f() {\r\n  int a, b;\r\n  int* p;\r\n}\r\n"
        p = self.src(data)
        o1 = data.find(b"int a, b;")
        o2 = data.find(b"p;")
        self.run_fixes([("readability-isolate-declaration", p, [(o1, 9, "int a;\n  int b;")]),
                        ("cppcoreguidelines-init-variables", p, [(o2 + 1, 0, " = NULL")])])
        self.assertEqual(p.read_bytes(),
                         b"// \xc2\xa9\r\nint f() {\r\n  int a;\r\n  int b;\r\n  int* p = NULL;\r\n}\r\n")

    def test_lf_file_stays_lf(self):
        data = b"int a, b;\n"
        p = self.src(data)
        self.run_fixes([("readability-isolate-declaration", p, [(0, 9, "int a;\nint b;")])])
        self.assertEqual(p.read_bytes(), b"int a;\nint b;\n")

    def test_header_duplicates_applied_once(self):
        p = self.src(b"bool b = 0;\r\n")
        d = [("modernize-use-bool-literals", p, [(9, 1, "false")])]
        dups, accepted, _ = self.run_fixes(d, d, d)  # three TUs including the header
        self.assertEqual(dups, 2)
        self.assertEqual(len(accepted), 1)
        self.assertEqual(p.read_bytes(), b"bool b = false;\r\n")

    def test_overlap_skips_whole_diagnostic(self):
        p = self.src(b"if (v.size() == 0) x = 0;\r\n")
        first = ("readability-container-size-empty", p, [(4, 13, "v.empty()")])
        # Second diagnostic has two edits; one overlaps the first diagnostic.
        second = ("other-check", p, [(4, 1, "w"), (23, 1, "1")])
        _, accepted, conflicting = self.run_fixes([first, second])
        self.assertEqual([d.check for d in conflicting], ["other-check"])
        self.assertEqual(p.read_bytes(), b"if (v.empty()) x = 0;\r\n")

    def test_same_offset_insertions_conflict(self):
        a = ayf.Replacement("x", 5, 0, "a")
        b = ayf.Replacement("x", 5, 0, "b")
        self.assertTrue(a.overlaps(b))
        self.assertFalse(ayf.Replacement("x", 0, 5, "").overlaps(ayf.Replacement("x", 5, 2, "")))

    def test_out_of_range_raises(self):
        with self.assertRaises(ValueError):
            ayf.apply_to_bytes(b"abc", [ayf.Replacement("x", 2, 5, "")])


class Vs2008FilterTests(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp())

    def tearDown(self):
        shutil.rmtree(str(self.dir))

    def check(self, line, offset_marker, text, check="cppcoreguidelines-init-variables"):
        p = self.dir / "b.cpp"
        data = ("void f() {\r\n" + line + "\r\n}\r\n").encode("utf-8")
        p.write_bytes(data)
        off = data.find(offset_marker.encode("utf-8")) + len(offset_marker)
        d = ayf.Diagnostic(check, "m", str(p), off, [ayf.Replacement(str(p), off, 0, text)])
        kept, filtered, converted = rct.vs2008_filter([d], rct.FileCache())
        return kept, filtered, converted, d

    def test_va_list_init_filtered(self):
        _, filtered, _, _ = self.check("\tva_list vl;", "va_list vl", " = NULL")
        self.assertEqual(len(filtered), 1)

    def test_aggregate_zero_filtered(self):
        _, filtered, _, _ = self.check("\tVoteCommitmentList vDesiredCommitments;", "vDesiredCommitments", " = 0")
        self.assertEqual(len(filtered), 1)

    def test_nan_filtered(self):
        _, filtered, _, _ = self.check("\tfloat f;", "float f", " = NAN")
        self.assertEqual(len(filtered), 1)

    def test_nullptr_converted(self):
        kept, filtered, converted, d = self.check("\tint* p;", "int* p", " = nullptr")
        self.assertEqual((len(kept), len(filtered), converted), (1, 0, 1))
        self.assertEqual(d.replacements[0].text, " = NULL")

    def test_plain_int_kept(self):
        kept, filtered, _, _ = self.check("\tint i;", "int i", " = 0")
        self.assertEqual((len(kept), len(filtered)), (1, 0))

    def test_nested_call_not_mistaken_for_corruption(self):
        # The old filter rejected any text matching \w+\(\)\); such as foo(bar());
        kept, filtered, _, _ = self.check("\tfoo(v.size());", "foo(", "", check="x")
        self.assertEqual(len(filtered), 0)

    def test_compiler_fixits_never_applied(self):
        _, filtered, _, _ = self.check("\tint i;", "int i", ";", check="clang-diagnostic-error")
        self.assertEqual(len(filtered), 1)


class CheckSelectionTests(unittest.TestCase):
    class A(object):
        checks = None
        add_checks = None

    def test_blacklisted_exact_refused(self):
        a = self.A()
        a.checks = "modernize-use-auto"
        with self.assertRaises(SystemExit):
            rct.resolve_checks(a)

    def test_glob_excludes_blacklisted(self):
        a = self.A()
        a.checks = "readability-*"
        checks = rct.resolve_checks(a)
        self.assertIn("-readability-implicit-bool-conversion", checks)
        self.assertIn("-readability-math-missing-parentheses", checks)

    def test_default_is_proven(self):
        self.assertEqual(rct.resolve_checks(self.A()), rct.PROVEN_CHECKS)


class MsvcLogTests(unittest.TestCase):
    def test_error_line_parsed(self):
        line = r"  1>c:\users\user\cpd\CvGameCoreDLL_Expansion2\CustomMods.cpp(41) : error C2552: 'args' : non-aggregates cannot be initialized with initializer list"
        m = rct.MSVC_ERROR.match(line)
        self.assertIsNotNone(m)
        self.assertTrue(m.group("file").endswith("CustomMods.cpp"))
        self.assertEqual((m.group("line"), m.group("code")), ("41", "C2552"))

    def test_warning_and_linker_lines_ignored(self):
        for line in [r"c:\x\a.cpp(3) : warning C4100: 'p' : unreferenced formal parameter",
                     r"LINK : fatal error LNK1104: cannot open file 'x.lib'"]:
            self.assertIsNone(rct.MSVC_ERROR.match(line))

    def test_msvc_unavailable_off_windows(self):
        if not rct.IS_WINDOWS:
            self.assertFalse(rct.msvc_available()[0])


if __name__ == "__main__":
    unittest.main()
