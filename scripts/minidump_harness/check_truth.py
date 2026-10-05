#!/usr/bin/env python3
"""Check analyze_minidump.py's unwound stacks against the harness ground truth.

Each harness dump comes with a .truth.txt listing every live frame's return
address (recorded with _ReturnAddress() before the crash). The recorded
return addresses (innermost first, tail-call duplicates collapsed) must
appear in the unwound stack in order, contiguously, at the expected frame.

Without the host image, only frames reachable through the DLL's PDB unwind
records are guaranteed; later EXE frames may be guesses, but then the report
must flag them [unverified].

Usage: check_truth.py   (after build.bat release and build.bat debug)
"""
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "..", "analyze_minidump.py")

# frame index where the first recorded return address must appear
# (stale: #0 is the faulting Live4_Write; nullcall/exenull: #0 is eip=0 and
# #1 the call site recovered from [esp]; cxx: system frames vary)
EXPECTED_FIRST = {"stale": 1, "nullcall": 2, "cxx": None, "exenull": 2}


def truth(path):
    rets, slots, head = [], set(), None
    for line in open(path):
        p = line.split()
        if p[0] == "exception":
            head = line.strip()
        elif p[0] == "frame":
            slot, ret = int(p[3], 16), int(p[5], 16)
            if slot not in slots:
                slots.add(slot)
                rets.append((p[1], ret))
    return head, rets


def analyze(cfg, scen, extra):
    d = os.path.join(HERE, cfg)
    dmp = os.path.join(d, "CvMiniDump_%s_%s.dmp" % (scen, cfg))
    out = subprocess.run(
        [sys.executable, TOOL, dmp, "--json",
         "--dll", os.path.join(d, "CvGameCore_Expansion2.dll"),
         "--pdb", os.path.join(d, "CvGameCore_Expansion2.pdb")] + extra,
        capture_output=True, text=True)
    return json.loads(out.stdout), dmp


def check(cfg, scen, with_image):
    extra = ["--image", os.path.join(HERE, cfg, "host.exe")] \
        if with_image else []
    rep, dmp = analyze(cfg, scen, extra)
    head, rets = truth(dmp.replace(".dmp", ".truth.txt"))
    eips = [int(f["eip"], 16) for f in rep["stack"]]
    want = [r for _, r in rets]
    if not with_image:
        dllbase = int(head.split("dllbase")[1].split()[0], 16)
        cut = next((k for k, r in enumerate(want)
                    if not dllbase <= r < dllbase + 0x100000), len(want) - 1)
        if cut == 0:
            # crash entirely inside the EXE: the report must say so
            return bool(rep.get("unverified_modules")), "EXE-only crash"
        full, want = want, want[:cut + 1]
        first = eips.index(full[0]) if full[0] in eips else -1
        if eips[first:first + len(full)] != full and \
                not rep.get("unverified_modules"):
            return False, "later frames differ without an unverified warning"
    first = eips.index(want[0]) if want[0] in eips else -1
    ok = first >= 0 and eips[first:first + len(want)] == want
    if ok and EXPECTED_FIRST[scen] is not None:
        ok = first == EXPECTED_FIRST[scen]
    return ok, "truth at frame %d" % first


def main():
    failures = 0
    for cfg in ("release", "debug"):
        for scen in ("stale", "nullcall", "cxx", "exenull"):
            for with_image in (True, False):
                ok, detail = check(cfg, scen, with_image)
                failures += not ok
                print("%s %-7s %-8s %-15s %s" % (
                    "PASS" if ok else "FAIL", cfg, scen,
                    "dll+exe image" if with_image else "dll only", detail))
    print("failures: %d" % failures)
    return failures


if __name__ == "__main__":
    sys.exit(1 if main() else 0)
