#!/usr/bin/env python3
"""
Minidump crash analyzer for CvGameCore_Expansion2.dll.

Parses CvMiniDump_*.dmp files (including Wine-generated dumps that break the
python `minidump` library with a nonstandard 0xfff0 stream), pairs the crash
with the matching DLL/PDB, symbolizes the faulting address and stack, and
prints a full crash report.

Symbolization backends (best available wins):
  1. llvm-symbolizer (LLVM >= 19; reads PDB natively, gives file:line and
     inlined frames). Located via --llvm-path, $LLVM_PATH/bin, or $PATH.
  2. Built-in pure-Python PDB reader (MSF -> DBI -> module symbol streams,
     S_GPROC32/S_LPROC32 + public symbols). No dependencies.

Usage:
    python3 scripts/analyze_minidump.py CRASH.dmp
    python3 scripts/analyze_minidump.py CRASH.dmp --symbols DIR
    python3 scripts/analyze_minidump.py CRASH.dmp --crashes-log crashes.log
    python3 scripts/analyze_minidump.py CRASH.dmp --rva 0xA57CF0

  --symbols DIR       Directory searched recursively for CvGameCore DLL/PDB
                      pairs (e.g. an extracted Release_Debug.zip). The right
                      pair is auto-selected by matching PE timestamp +
                      SizeOfImage against the module in the dump, then the
                      RSDS GUID pairs DLL <-> PDB.
  --pdb PDB --dll DLL Explicit pair (skips auto-matching; mismatch = warning).
  --crashes-log FILE  Cross-check against crashes.log ("Location (in file)"
                      is a FILE OFFSET; true RVA = offset + .text raw/virtual
                      delta, typically +0xC00).
  --rva HEX           Symbolize extra RVAs (repeatable) in the target module.
  --image FILE        Extra module image on disk (e.g. the game exe) used to
                      verify stack-scan candidates as real call return
                      addresses and to translate crashes.log file offsets
                      for that module (repeatable).
  --max-frames N      Stack-scan frame limit (default 40).
  --json              Machine-readable output.

Additional automatic analysis:
  * WINE/Proton detection from the dump's module list (wined3d.dll etc.).
  * crashes.log "???+0xfffffXXX" entries decoded as a wrapped NULL/garbage
    EIP (call through a bad function pointer).
  * 0xE06D7363 MSVC C++ exceptions: the thrown type is recovered from the
    ThrowInfo chain when the pages were captured in the dump.
  * Empty/truncated dumps are reported as MiniDumpWriteDump failures
    (typically OOM during capture) instead of a generic parse error.
  * Warns when the matched crashes.log entry is for a different dump file.

Exit codes: 0 ok, 1 bad input, 2 dump parsed but no symbols found.
"""

import argparse
import json
import os
import re
import struct
import subprocess
import sys

MINIDUMP_SIGNATURE = 0x504D444D  # 'MDMP'
STREAM_THREAD_LIST = 3
STREAM_MODULE_LIST = 4
STREAM_MEMORY_LIST = 5
STREAM_EXCEPTION = 6
STREAM_SYSTEM_INFO = 7
STREAM_SYSTEM_MEMORY_INFO = 21   # MINIDUMP_SYSTEM_MEMORY_INFO_1
STREAM_PROCESS_VM_COUNTERS = 22  # MINIDUMP_PROCESS_VM_COUNTERS_2

MSVC_CXX_EXCEPTION = 0xE06D7363
MSVC_CXX_MAGIC = 0x19930520

EXCEPTION_NAMES = {
    0xC0000005: "Access Violation",
    0xC0000094: "Integer Divide by Zero",
    0xC00000FD: "Stack Overflow",
    0xC0000409: "Stack Buffer Overrun / Fail Fast",
    0xC0000374: "Heap Corruption",
    0x80000003: "Breakpoint",
    0xC000001D: "Illegal Instruction",
    0xE06D7363: "C++ Exception (MSVC)",
}

AV_KIND = {0: "read", 1: "write", 8: "execute (DEP)"}


def fail(msg):
    sys.stderr.write("error: %s\n" % msg)
    sys.exit(1)


# ---------------------------------------------------------------------------
# Minidump parsing (manual struct walk; Wine dumps have a nonstandard 0xfff0
# stream that trips strict parsers, so never validate stream types).
# ---------------------------------------------------------------------------

class Minidump(object):
    def __init__(self, path):
        self.path = path
        with open(path, "rb") as f:
            self.data = f.read()
        d = self.data
        if len(d) == 0:
            fail("%s: empty dump -- MiniDumpWriteDump itself failed, "
                 "usually because the process was out of memory (see the "
                 "matching crashes.log memory markers)" % path)
        if len(d) < 32:
            fail("%s: too small to be a minidump (truncated write -- "
                 "likely OOM during dump capture)" % path)
        sig, _ver, nstreams, dirrva = struct.unpack_from("<IIII", d, 0)
        if sig != MINIDUMP_SIGNATURE:
            fail("%s: bad signature 0x%08x (not MDMP)" % (path, sig))
        self.flags = struct.unpack_from("<Q", d, 24)[0]
        self.streams = {}
        for i in range(nstreams):
            stype, size, rva = struct.unpack_from("<III", d, dirrva + 12 * i)
            if stype:
                self.streams.setdefault(stype, (size, rva))
        self.modules = self._parse_modules()
        self.threads = self._parse_threads()
        self.memory_ranges = self._parse_memory_list()
        self.exception = self._parse_exception()
        self.system_info = self._parse_system_info()
        self.system_memory = self._parse_system_memory_info()
        self.vm_counters = self._parse_process_vm_counters()

    def _stream(self, stype):
        if stype not in self.streams:
            return None
        size, rva = self.streams[stype]
        return self.data[rva:rva + size]

    def _read_minidump_string(self, rva):
        if rva == 0 or rva + 4 > len(self.data):
            return ""
        n = struct.unpack_from("<I", self.data, rva)[0]
        raw = self.data[rva + 4:rva + 4 + n]
        return raw.decode("utf-16le", errors="replace")

    def _parse_modules(self):
        s = self._stream(STREAM_MODULE_LIST)
        if not s:
            return []
        n = struct.unpack_from("<I", s, 0)[0]
        mods = []
        for i in range(n):
            e = s[4 + i * 108: 4 + (i + 1) * 108]
            base, size, _cksum, timestamp, name_rva = \
                struct.unpack_from("<QIIII", e, 0)
            name = self._read_minidump_string(name_rva)
            mods.append({
                "base": base,
                "size": size,
                "timestamp": timestamp,
                "path": name,
                "name": name.replace("\\", "/").rsplit("/", 1)[-1],
            })
        mods.sort(key=lambda m: m["base"])
        return mods

    def _parse_threads(self):
        s = self._stream(STREAM_THREAD_LIST)
        if not s:
            return []
        n = struct.unpack_from("<I", s, 0)[0]
        threads = []
        for i in range(n):
            e = s[4 + i * 48: 4 + (i + 1) * 48]
            tid = struct.unpack_from("<I", e, 0)[0]
            stack_start = struct.unpack_from("<Q", e, 24)[0]
            stack_size, stack_rva = struct.unpack_from("<II", e, 36)
            threads.append({
                "tid": tid,
                "stack_start": stack_start,
                "stack_size": stack_size,
                "stack_rva": stack_rva,
            })
        return threads

    def _parse_memory_list(self):
        """MINIDUMP_MEMORY_LIST: captured VA ranges (thread stacks + extras).
        Enables read_va() for arbitrary captured addresses, e.g. C++
        exception object internals."""
        s = self._stream(STREAM_MEMORY_LIST)
        if not s:
            return []
        n = struct.unpack_from("<I", s, 0)[0]
        ranges = []
        for i in range(n):
            start, size, rva = struct.unpack_from("<QII", s, 4 + 16 * i)
            ranges.append((start, size, rva))
        return ranges

    def read_va(self, addr, size):
        """Read bytes at a virtual address if captured in the dump."""
        for start, rsize, rva in self.memory_ranges:
            if start <= addr and addr + size <= start + rsize:
                off = rva + (addr - start)
                return self.data[off:off + size]
        return None

    def read_u32(self, addr):
        b = self.read_va(addr, 4)
        return struct.unpack("<I", b)[0] if b else None

    def read_cstring(self, addr, maxlen=256):
        b = self.read_va(addr, maxlen)
        if not b:
            # retry with whatever tail of the range is available
            for n in (128, 64, 32, 16):
                b = self.read_va(addr, n)
                if b:
                    break
        if not b:
            return None
        s = b.split(b"\0")[0]
        try:
            return s.decode("ascii")
        except UnicodeDecodeError:
            return None

    def _parse_exception(self):
        s = self._stream(STREAM_EXCEPTION)
        if not s:
            return None
        tid = struct.unpack_from("<I", s, 0)[0]
        code, _flags, _rec, addr, nparams = struct.unpack_from("<IIQQI", s, 8)
        params = struct.unpack_from("<15Q", s, 8 + 28 + 4)[:nparams]
        ctx_size, ctx_rva = struct.unpack_from("<II", s, 160)
        regs = {}
        ctx = self.data[ctx_rva:ctx_rva + ctx_size]
        if len(ctx) >= 0x9C + 48:  # x86 CONTEXT: integer regs at offset 0x9C
            names = ["edi", "esi", "ebx", "edx", "ecx", "eax",
                     "ebp", "eip", "cs", "eflags", "esp", "ss"]
            vals = struct.unpack_from("<12I", ctx, 0x9C)
            regs = dict(zip(names, vals))
        return {
            "tid": tid,
            "code": code,
            "address": addr,
            "params": list(params),
            "registers": regs,
        }

    def _parse_system_info(self):
        s = self._stream(STREAM_SYSTEM_INFO)
        if not s:
            return {}
        arch = struct.unpack_from("<H", s, 0)[0]
        major, minor, build = struct.unpack_from("<III", s, 8)
        return {"arch": arch, "os": "%d.%d build %d" % (major, minor, build)}

    def _parse_system_memory_info(self):
        """System commit/RAM state at crash time (stream 21, Win8.1+).

        Rules out (or confirms) system commit-limit exhaustion, which the
        VA-only crashes.log markers cannot distinguish from VA exhaustion
        (proven necessary by issue #13262).
        """
        s = self._stream(STREAM_SYSTEM_MEMORY_INFO)
        # header (USHORT Revision, USHORT Flags) + SYSTEM_BASIC_INFORMATION
        # (64) + SYSTEM_FILECACHE_INFORMATION (48) + the 4 leading ULONG64s
        # of SYSTEM_BASIC_PERFORMANCE_INFORMATION
        if not s or len(s) < 4 + 112 + 32:
            return {}
        rev = struct.unpack_from("<H", s, 0)[0]
        if rev < 1:
            return {}
        page = struct.unpack_from("<I", s, 4 + 4)[0]
        phys_pages = struct.unpack_from("<I", s, 4 + 8)[0]
        avail, committed, limit, peak = struct.unpack_from("<4Q", s, 4 + 112)
        # content sanity (stream ids can be nonstandard; never trust blindly)
        if page not in (4096, 8192) or not (0 < committed <= limit < 2 ** 40):
            return {}
        return {
            "page_size": page,
            "phys_mb": phys_pages * page // 2 ** 20,
            "commit_available_mb": (limit - committed) * page // 2 ** 20,
            "commit_used_mb": committed * page // 2 ** 20,
            "commit_limit_mb": limit * page // 2 ** 20,
            "commit_peak_mb": peak * page // 2 ** 20,
            "phys_avail_mb": avail * page // 2 ** 20,
        }

    def _parse_process_vm_counters(self):
        """Crashed process VM counters (stream 22, Win10+)."""
        s = self._stream(STREAM_PROCESS_VM_COUNTERS)
        if not s or len(s) < 8:
            return {}
        rev, flags = struct.unpack_from("<HH", s, 0)
        # MINIDUMP_PROCESS_VM_COUNTERS_2: USHORT Revision, Flags;
        # ULONG PageFaultCount; then ULONG64 fields whose PRESENCE depends
        # on Flags (KAPI_FLAG bits): 0x1 basic 8 fields, 0x2 VirtualSize
        # pair, 0x4 PrivateUsage, 0x8 EX2 pair (+ 0x10 job fields).
        if rev < 2 or not (flags & 0x1):
            return {}
        names = ["peak_ws_mb", "ws_mb", None, None, None, None,
                 "pagefile_mb", "peak_pagefile_mb"]
        if flags & 0x2:
            names += ["peak_virtual_mb", "virtual_mb"]
        if flags & 0x4:
            names += ["private_mb"]
        if flags & 0x8:
            names += ["private_ws_mb", "shared_commit_mb"]
        if len(s) < 8 + 8 * len(names):
            return {}
        vals = struct.unpack_from("<%dQ" % len(names), s, 8)
        out = {}
        for n, v in zip(names, vals):
            if n:
                out[n] = v // 2 ** 20
        # content sanity: a 32-bit process commits < 4 GB
        if not (0 < out.get("pagefile_mb", 0) < 4096):
            return {}
        return out

    def module_for(self, addr):
        for m in self.modules:
            if m["base"] <= addr < m["base"] + m["size"]:
                return m
        return None

    def thread_stack(self, thread):
        return self.data[thread["stack_rva"]:
                         thread["stack_rva"] + thread["stack_size"]]


# ---------------------------------------------------------------------------
# PE / PDB pairing
# ---------------------------------------------------------------------------

def pe_info(path):
    """Return dict with timestamp, size_of_image, rsds guid+age, sections."""
    with open(path, "rb") as f:
        d = f.read()
    if d[:2] != b"MZ":
        return None
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    if d[pe:pe + 4] != b"PE\0\0":
        return None
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    timestamp = struct.unpack_from("<I", d, pe + 8)[0]
    optsize = struct.unpack_from("<H", d, pe + 20)[0]
    size_of_image = struct.unpack_from("<I", d, pe + 24 + 56)[0]
    secs_off = pe + 24 + optsize
    sections = []
    for i in range(nsec):
        s = secs_off + 40 * i
        name = d[s:s + 8].rstrip(b"\0").decode("ascii", errors="replace")
        vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", d, s + 8)
        sections.append({"name": name, "va": va, "vsize": vsize,
                         "rawptr": rawptr, "rawsize": rawsize})
    # Debug directory -> RSDS record
    ddir_rva, ddir_size = struct.unpack_from("<II", d, pe + 24 + 96 + 6 * 8)
    guid = age = pdb_name = None
    if ddir_rva:
        off = rva_to_off(sections, ddir_rva)
        if off is not None:
            for i in range(ddir_size // 28):
                dtype = struct.unpack_from("<I", d, off + 28 * i + 12)[0]
                if dtype != 2:  # IMAGE_DEBUG_TYPE_CODEVIEW
                    continue
                praw = struct.unpack_from("<I", d, off + 28 * i + 24)[0]
                if d[praw:praw + 4] == b"RSDS":
                    guid = d[praw + 4:praw + 20].hex().upper()
                    age = struct.unpack_from("<I", d, praw + 20)[0]
                    pdb_name = d[praw + 24:d.index(b"\0", praw + 24)] \
                        .decode(errors="replace")
                break
    return {"timestamp": timestamp, "size_of_image": size_of_image,
            "sections": sections, "guid": guid, "age": age,
            "pdb_name": pdb_name, "path": path}


def rva_to_off(sections, rva):
    for s in sections:
        if s["va"] <= rva < s["va"] + max(s["vsize"], s["rawsize"]):
            return rva - s["va"] + s["rawptr"]
    return None


def pdb_guid(path):
    """GUID+age from a PDB's info stream (stream 1). Pure Python."""
    try:
        msf = MsfFile(path)
        info = msf.stream(1)
        age = struct.unpack_from("<I", info, 8)[0]
        return info[12:28].hex().upper(), age
    except Exception:
        return None, None


def find_symbol_pair(search_dir, module):
    """Find (dll, pdb) in search_dir matching module timestamp+SizeOfImage."""
    dlls, pdbs = [], []
    for root, _dirs, files in os.walk(search_dir):
        for fn in files:
            p = os.path.join(root, fn)
            if fn.lower().endswith(".dll"):
                dlls.append(p)
            elif fn.lower().endswith(".pdb"):
                pdbs.append(p)
    for dll in dlls:
        info = pe_info(dll)
        if not info or info["timestamp"] != module["timestamp"] \
                or info["size_of_image"] != module["size"]:
            continue
        # Prefer the PDB sitting next to the DLL, then GUID-match the rest.
        candidates = [p for p in pdbs
                      if os.path.dirname(p) == os.path.dirname(dll)] + pdbs
        for pdb in candidates:
            g, a = pdb_guid(pdb)
            if g and g == info["guid"] and a == info["age"]:
                return dll, pdb, info
    return None, None, None


# ---------------------------------------------------------------------------
# Pure-Python PDB reader (fallback when llvm-symbolizer is unavailable)
# ---------------------------------------------------------------------------

class MsfFile(object):
    def __init__(self, path):
        with open(path, "rb") as f:
            self.data = f.read()
        d = self.data
        if not d.startswith(b"Microsoft C/C++ MSF 7.00"):
            raise ValueError("not an MSF 7.00 PDB")
        self.page_size, _free, _ntot, dirsize = \
            struct.unpack_from("<IIII", d, 32)
        psz = self.page_size

        def pages(n):
            return (n + psz - 1) // psz

        ndirpg = pages(dirsize)
        npp = pages(ndirpg * 4)
        pp = struct.unpack_from("<%dI" % npp, d, 52)
        dirpgs = []
        for p in pp:
            dirpgs += list(struct.unpack_from("<%dI" % (psz // 4), d, p * psz))
        dirpgs = dirpgs[:ndirpg]
        dirdat = b"".join(d[p * psz:(p + 1) * psz] for p in dirpgs)[:dirsize]
        nstreams = struct.unpack_from("<I", dirdat, 0)[0]
        sizes = struct.unpack_from("<%dI" % nstreams, dirdat, 4)
        off = 4 + 4 * nstreams
        self._streams = []
        for s in sizes:
            if s in (0xFFFFFFFF, 0):
                self._streams.append((0, []))
                continue
            np_ = pages(s)
            pg = struct.unpack_from("<%dI" % np_, dirdat, off)
            off += 4 * np_
            self._streams.append((s, list(pg)))

    def stream(self, i):
        size, pg = self._streams[i]
        if not pg:
            return b""
        psz = self.page_size
        return b"".join(self.data[p * psz:(p + 1) * psz] for p in pg)[:size]


class PdbSymbols(object):
    """Function symbols (rva, length, name) from DBI module streams + publics."""

    S_PUB32 = 0x110E
    S_LPROC32 = 0x110F
    S_GPROC32 = 0x1110

    def __init__(self, path):
        self.msf = MsfFile(path)
        dbi = self.msf.stream(3)
        modinfo_sz, seccon_sz, secmap_sz, srcinfo_sz, tsmap_sz = \
            struct.unpack_from("<IIIII", dbi, 24)
        dbghdr_sz = struct.unpack_from("<I", dbi, 48)[0]
        ec_sz = struct.unpack_from("<I", dbi, 52)[0]
        symrec_stream = struct.unpack_from("<H", dbi, 20)[0]
        # Optional debug header: index 5 (uint16) = section header stream
        dbgoff = 64 + modinfo_sz + seccon_sz + secmap_sz + srcinfo_sz \
            + tsmap_sz + ec_sz
        dbghdr = dbi[dbgoff:dbgoff + dbghdr_sz]
        idx_secthdr = struct.unpack_from("<H", dbghdr, 10)[0]
        sect = self.msf.stream(idx_secthdr)
        self.sec_rva = [struct.unpack_from("<I", sect, i * 40 + 12)[0]
                        for i in range(len(sect) // 40)]
        self.procs = []    # (rva, length, name)
        self.publics = []  # (rva, name)
        self._parse_modules(dbi, modinfo_sz)
        self._parse_publics(self.msf.stream(symrec_stream))
        self.procs.sort()
        self.publics.sort()

    def _seg_rva(self, seg, off):
        if 1 <= seg <= len(self.sec_rva):
            return self.sec_rva[seg - 1] + off
        return None

    def _parse_modules(self, dbi, modinfo_sz):
        off = 64
        end = 64 + modinfo_sz
        while off < end:
            stream = struct.unpack_from("<H", dbi, off + 34)[0]
            symbytes = struct.unpack_from("<I", dbi, off + 36)[0]
            p = off + 64
            p = dbi.index(b"\0", p) + 1  # module name
            p = dbi.index(b"\0", p) + 1  # obj file name
            if p % 4:
                p += 4 - (p % 4)
            if stream != 0xFFFF and symbytes > 4:
                self._parse_proc_syms(self.msf.stream(stream), symbytes)
            off = p

    def _parse_proc_syms(self, sym, symbytes):
        so = 4
        while so + 4 <= symbytes:
            ln, typ = struct.unpack_from("<HH", sym, so)
            if ln < 2:
                break
            if typ in (self.S_GPROC32, self.S_LPROC32):
                plen = struct.unpack_from("<I", sym, so + 16)[0]
                soff = struct.unpack_from("<I", sym, so + 32)[0]
                seg = struct.unpack_from("<H", sym, so + 36)[0]
                name = sym[so + 39:so + 2 + ln].split(b"\0")[0]
                rva = self._seg_rva(seg, soff)
                if rva is not None:
                    self.procs.append(
                        (rva, plen, name.decode("utf-8", errors="replace")))
            so += 2 + ln

    def _parse_publics(self, syms):
        off = 0
        while off + 4 <= len(syms):
            ln, typ = struct.unpack_from("<HH", syms, off)
            if ln < 2:
                break
            if typ == self.S_PUB32:
                _flags, soff, seg = struct.unpack_from("<IIH", syms, off + 4)
                name = syms[off + 14:off + 2 + ln].split(b"\0")[0]
                rva = self._seg_rva(seg, soff)
                if rva is not None:
                    self.publics.append(
                        (rva, name.decode("utf-8", errors="replace")))
            off += 2 + ln

    def symbolize(self, rva):
        for prva, plen, name in self.procs:
            if prva <= rva < prva + plen:
                return "%s+0x%x" % (name, rva - prva)
        best = None
        for prva, name in self.publics:
            if prva <= rva and (best is None or prva > best[0]):
                best = (prva, name)
        if best:
            return "%s+0x%x (nearest public)" % (best[1], rva - best[0])
        return None


# ---------------------------------------------------------------------------
# llvm-symbolizer backend
# ---------------------------------------------------------------------------

def find_llvm_symbolizer(explicit_path):
    candidates = []
    if explicit_path:
        candidates.append(os.path.join(explicit_path, "bin",
                                       "llvm-symbolizer"))
        candidates.append(os.path.join(explicit_path, "llvm-symbolizer"))
    env = os.environ.get("LLVM_PATH")
    if env:
        candidates.append(os.path.join(env, "bin", "llvm-symbolizer"))
    for name in ("llvm-symbolizer", "llvm-symbolizer-22",
                 "llvm-symbolizer-21", "llvm-symbolizer-20",
                 "llvm-symbolizer-19"):
        candidates.append(name)
    for c in candidates:
        try:
            subprocess.run([c, "--version"], capture_output=True, check=True)
            return c
        except (OSError, subprocess.CalledProcessError):
            continue
    return None


def llvm_symbolize(symbolizer, dll, rvas):
    """Symbolize RVAs; returns {rva: [(func, file:line), ...]} incl. inlines."""
    if not rvas:
        return {}
    inp = "".join("0x%X\n" % r for r in rvas)
    proc = subprocess.run(
        [symbolizer, "--obj=%s" % dll, "--relative-address",
         "--output-style=LLVM"],
        input=inp, capture_output=True, text=True)
    results = {}
    blocks = proc.stdout.rstrip("\n").split("\n\n")
    for rva, block in zip(rvas, blocks):
        frames = []
        lines = block.split("\n")
        for j in range(0, len(lines) - 1, 2):
            func, loc = lines[j].strip(), lines[j + 1].strip()
            if func and func != "??":
                frames.append((func, loc))
        results[rva] = frames
    return results


# ---------------------------------------------------------------------------
# Stack scanning (no unwind info on x86; conservative return-address scan)
# ---------------------------------------------------------------------------

def scan_stack(dump, thread, from_addr, max_frames, checkers=None):
    """Scan thread stack upward from from_addr for module return addresses.

    checkers maps a lowercase module name to a callable(rva) -> bool that
    validates candidates (e.g. "lands in an executable section and is
    preceded by a call instruction", built from the module's file on disk).
    Modules without a checker are reported unfiltered.
    """
    stack = dump.thread_stack(thread)
    start = thread["stack_start"]
    frames = []
    begin = max(0, from_addr - start)
    begin -= begin % 4
    for k in range(begin, len(stack) - 3, 4):
        v = struct.unpack_from("<I", stack, k)[0]
        m = dump.module_for(v)
        if not m:
            continue
        check = (checkers or {}).get(m["name"].lower())
        if check and not check(v - m["base"]):
            continue
        frames.append({
            "stack_addr": start + k,
            "value": v,
            "module": m["name"],
            "rva": v - m["base"],
        })
        if len(frames) >= max_frames:
            break
    return frames


IMAGE_SCN_MEM_EXECUTE = 0x20000000


def make_return_address_checker(dll_path):
    """Candidate return addresses must land in an executable section and be
    preceded by a plausible x86 call instruction (E8 rel32, or the FF /2
    indirect forms). Only possible for modules whose file we have on disk."""
    with open(dll_path, "rb") as f:
        data = f.read()
    exec_secs = []
    peh = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, peh + 6)[0]
    optsize = struct.unpack_from("<H", data, peh + 20)[0]
    for i in range(nsec):
        s = peh + 24 + optsize + 40 * i
        chars = struct.unpack_from("<I", data, s + 36)[0]
        if chars & IMAGE_SCN_MEM_EXECUTE:
            vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", data,
                                                            s + 8)
            exec_secs.append((va, min(vsize, rawsize), rawptr))

    def check(rva):
        for va, size, rawptr in exec_secs:
            if va <= rva < va + size:
                off = rva - va + rawptr
                if off < 7:
                    return False
                b = data[off - 7:off]
                # call rel32 (E8) / call r/m32 (FF /2: 2-7 byte encodings)
                if b[2] == 0xE8:
                    return True
                for ln in (2, 3, 4, 5, 6, 7):
                    i = 7 - ln
                    if b[i] == 0xFF and (b[i + 1] >> 3) & 7 == 2:
                        return True
                return False
        return False

    return check


# ---------------------------------------------------------------------------
# MSVC C++ exception (0xE06D7363) decoding
# ---------------------------------------------------------------------------

def decode_msvc_exception(dump, exc):
    """Decode the thrown C++ type from an MSVC 0xE06D7363 exception.

    x86 exception parameters: [magic 0x19930520, pObject, pThrowInfo].
    Follows ThrowInfo -> CatchableTypeArray -> CatchableType ->
    TypeDescriptor to recover the mangled type name (all VAs; only works
    if those pages were captured in the dump's memory list)."""
    params = exc.get("params") or []
    if exc.get("code") != MSVC_CXX_EXCEPTION or len(params) < 3:
        return None
    out = {"magic": "0x%X" % params[0], "object": "0x%X" % params[1],
           "throw_info": "0x%X" % params[2]}
    if params[0] != MSVC_CXX_MAGIC:
        out["note"] = "unexpected magic (not a standard MSVC throw)"
        return out
    ti = params[2]
    cta = dump.read_u32(ti + 12)          # pCatchableTypeArray
    if cta is None:
        out["note"] = "ThrowInfo memory not captured in dump"
        return out
    ncat = dump.read_u32(cta)
    types = []
    for i in range(min(ncat or 0, 8)):
        ct = dump.read_u32(cta + 4 + 4 * i)   # pCatchableType
        if not ct:
            continue
        td = dump.read_u32(ct + 4)            # pType (TypeDescriptor)
        if not td:
            continue
        name = dump.read_cstring(td + 8)      # mangled name after vftable+spare
        if name:
            types.append(name)
    if types:
        out["types"] = types
        # ".?AVCvException@@" -> "CvException" (best-effort demangle)
        m = re.match(r"\.\?A[VU](\w+)@", types[0])
        if m:
            out["thrown_type"] = m.group(1)
    else:
        out["note"] = "type descriptors not captured in dump"
    return out


# ---------------------------------------------------------------------------
# crashes.log parsing
# ---------------------------------------------------------------------------

def parse_crashes_log(path, dump_name):
    """Return the crashes.log entry matching the dump filename (or last)."""
    with open(path, "r", errors="replace") as f:
        text = f.read()
    entries = [e for e in re.split(r"--Crash details--", text) if e.strip()]
    chosen = None
    for e in entries:
        if dump_name and dump_name in e:
            chosen = e
            break
    if chosen is None and entries:
        chosen = entries[-1]
    if not chosen:
        return None
    out = {}
    m = re.search(r"Location \(in file\): (\S+)\+0x([0-9a-fA-F]+)", chosen)
    if m:
        out["module"] = m.group(1)
        out["file_offset"] = int(m.group(2), 16)
        # "???+0xfffffXXX" = EIP outside any module (e.g. call through a
        # NULL/garbage function pointer); the unsigned file-offset
        # subtraction wrapped around 0.  eip = offset - 0x100000000 + 0xC00.
        if out["module"] == "???" and out["file_offset"] >= 0xFFFF0000:
            out["wrapped_eip"] = (out["file_offset"] + 0xC00) & 0xFFFFFFFF
    m = re.search(r"Minidump: (\S+)", chosen)
    if m:
        out["minidump_name"] = m.group(1)
    m = re.search(r"OS Info: (.+)", chosen)
    if m:
        out["os_info"] = m.group(1).strip()
    m = re.search(r"DLL-Version: (.+)", chosen)
    if m:
        out["dll_version"] = m.group(1).strip()
    m = re.search(r"Configuration: (\S+)", chosen)
    if m:
        out["configuration"] = m.group(1)
    m = re.search(r"Largest free block.*?Sub2G: 0x[0-9a-fA-F]+ / (\d+)",
                  chosen, re.S)
    if m:
        out["largest_free_sub2g_k"] = int(m.group(1))
    m = re.search(r"Largest free block.*?Total: 0x([0-9a-fA-F]+) / (\d+)",
                  chosen, re.S)
    if m:
        out["largest_free_total_base"] = int(m.group(1), 16)
        out["largest_free_total_k"] = int(m.group(2))
    m = re.search(r"committed\(k\).*?\nSub2G: (\d+) / (\d+) / (\d+)", chosen)
    if m:
        out["sub2g_committed_k"] = int(m.group(1))
        out["sub2g_free_k"] = int(m.group(3))
    return out


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------

def text_offset_delta(pe):
    """RVA - file_offset delta for the .text section (crashes.log fixup)."""
    for s in pe["sections"]:
        if s["name"] == ".text":
            return s["va"] - s["rawptr"]
    return None


def main():
    ap = argparse.ArgumentParser(
        description="Analyze a CvMiniDump crash dump and symbolize it "
                    "against the matching PDB.")
    ap.add_argument("dump", help="CvMiniDump_*.dmp file")
    ap.add_argument("--symbols", metavar="DIR",
                    help="directory searched recursively for matching "
                         "DLL/PDB (e.g. extracted Release_Debug.zip)")
    ap.add_argument("--dll", help="explicit DLL path")
    ap.add_argument("--pdb", help="explicit PDB path")
    ap.add_argument("--crashes-log", help="crashes.log for cross-checking")
    ap.add_argument("--llvm-path", help="LLVM install root "
                                        "(default: $LLVM_PATH, then $PATH)")
    ap.add_argument("--rva", action="append", default=[],
                    help="extra RVA(s) to symbolize (hex)")
    ap.add_argument("--image", action="append", default=[],
                    help="additional module image on disk (e.g. the game "
                         "exe) used to verify stack-scan candidates as real "
                         "call return addresses (repeatable)")
    ap.add_argument("--module", default="CvGameCore_Expansion2.dll",
                    help="target module name (default: %(default)s)")
    ap.add_argument("--max-frames", type=int, default=40)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    if not os.path.isfile(args.dump):
        fail("dump not found: %s" % args.dump)

    dump = Minidump(args.dump)
    wine_modules = sorted(
        m["name"] for m in dump.modules
        if m["name"].lower() in ("wined3d.dll", "winex11.drv",
                                 "winevulkan.dll", "winegstreamer.dll",
                                 "winemac.drv", "winewayland.drv",
                                 "lsteamclient.dll"))
    report = {"dump": os.path.basename(args.dump),
              "modules": len(dump.modules), "threads": len(dump.threads),
              "wine": bool(wine_modules), "wine_modules": wine_modules}

    target_mod = None
    for m in dump.modules:
        if m["name"].lower() == args.module.lower():
            target_mod = m
            break

    exc = dump.exception
    crash_rva = None
    crash_mod = None
    if exc:
        crash_mod = dump.module_for(exc["address"])
        if crash_mod:
            crash_rva = exc["address"] - crash_mod["base"]
        report["exception"] = {
            "code": "0x%08X" % exc["code"],
            "name": EXCEPTION_NAMES.get(exc["code"], "unknown"),
            "tid": exc["tid"],
            "address": "0x%X" % exc["address"],
            "module": crash_mod["name"] if crash_mod else None,
            "rva": ("0x%X" % crash_rva) if crash_rva is not None else None,
            "registers": {k: "0x%08X" % v
                          for k, v in exc["registers"].items()},
        }
        if exc["code"] == 0xC0000005 and len(exc["params"]) >= 2:
            report["exception"]["access"] = "%s of address 0x%X" % (
                AV_KIND.get(exc["params"][0], "op %d" % exc["params"][0]),
                exc["params"][1])
        cxx = decode_msvc_exception(dump, exc)
        if cxx:
            report["exception"]["cxx"] = cxx

    # Symbols: explicit pair or auto-match against a search directory.
    dll = args.dll
    pdb = args.pdb
    pe = None
    if dll:
        pe = pe_info(dll)
        if not pe:
            fail("not a PE file: %s" % dll)
        if target_mod and (pe["timestamp"] != target_mod["timestamp"]
                           or pe["size_of_image"] != target_mod["size"]):
            sys.stderr.write(
                "warning: %s timestamp/SizeOfImage does not match the module "
                "in the dump -- symbols may be wrong\n" % dll)
        if pdb:
            g, a = pdb_guid(pdb)
            if g and pe["guid"] and (g != pe["guid"] or a != pe["age"]):
                sys.stderr.write(
                    "warning: PDB GUID/age does not match DLL RSDS record\n")
    elif args.symbols:
        if not target_mod:
            fail("module %s not present in dump; cannot auto-match symbols"
                 % args.module)
        dll, pdb, pe = find_symbol_pair(args.symbols, target_mod)
        if not dll:
            sys.stderr.write(
                "warning: no DLL in %s matches module timestamp 0x%08X + "
                "SizeOfImage 0x%X\n" % (args.symbols,
                                        target_mod["timestamp"],
                                        target_mod["size"]))
    if dll and not pdb and pe and pe["pdb_name"]:
        cand = os.path.join(
            os.path.dirname(dll),
            os.path.basename(pe["pdb_name"].replace("\\", "/")))
        if os.path.isfile(cand):
            pdb = cand
    report["symbols"] = {"dll": dll, "pdb": pdb,
                         "guid": pe["guid"] if pe else None}

    if dump.system_memory:
        report["system_memory"] = dump.system_memory
    if dump.vm_counters:
        report["vm_counters"] = dump.vm_counters

    if args.crashes_log:
        entry = parse_crashes_log(args.crashes_log,
                                  os.path.basename(args.dump))
        if entry:
            report["crashes_log"] = entry
            if entry.get("minidump_name") and \
                    entry["minidump_name"] != os.path.basename(args.dump):
                entry["note_mismatch"] = (
                    "crashes.log entry is for %s, not this dump"
                    % entry["minidump_name"])
            # File-offset -> RVA fixup for any module image we have on disk
            # (the target DLL or an --image exe). "???" entries (EIP outside
            # any module, e.g. a call to NULL) wrap around 0 and carry the
            # raw eip instead.
            log_mod = entry.get("module", "").lower()
            fix_pe = None
            if pe and log_mod == args.module.lower():
                fix_pe = pe
            else:
                for img in args.image:
                    if os.path.basename(img).lower() == log_mod:
                        fix_pe = pe_info(img)
                        break
            if fix_pe and "file_offset" in entry \
                    and "wrapped_eip" not in entry:
                delta = text_offset_delta(fix_pe)
                if delta is not None:
                    entry["true_rva"] = "0x%X" % (
                        (entry["file_offset"] + delta) & 0xFFFFFFFF)
                    entry["note"] = (
                        "'Location (in file)' is a file offset; +0x%X "
                        "(.text raw->virtual) = true RVA" % delta)

    rvas = []
    if crash_rva is not None and crash_mod and \
            crash_mod["name"].lower() == args.module.lower():
        rvas.append(crash_rva)
    for r in args.rva:
        rvas.append(int(r, 16))

    stack_frames = []
    if exc:
        thread = next((t for t in dump.threads
                       if t["tid"] == exc["tid"]), None)
        if thread and thread["stack_size"]:
            esp = exc["registers"].get("esp", thread["stack_start"])
            checkers = {}
            if dll:
                try:
                    checkers[args.module.lower()] = \
                        make_return_address_checker(dll)
                except Exception:
                    pass
            for img in args.image:
                name = os.path.basename(img).lower()
                mod = next((m for m in dump.modules
                            if m["name"].lower() == name), None)
                info = pe_info(img)
                if not mod or not info:
                    sys.stderr.write("warning: --image %s: no matching "
                                     "module in dump or not a PE file\n"
                                     % img)
                    continue
                if info["timestamp"] != mod["timestamp"] \
                        or info["size_of_image"] != mod["size"]:
                    sys.stderr.write(
                        "warning: --image %s does not match the module in "
                        "the dump (timestamp/SizeOfImage); skipping\n" % img)
                    continue
                checkers[name] = make_return_address_checker(img)
            stack_frames = scan_stack(dump, thread, esp, args.max_frames,
                                      checkers)
            for f in stack_frames:
                if f["module"].lower() == args.module.lower():
                    rvas.append(f["rva"])

    symmap = {}
    backend = None
    if dll and rvas:
        symbolizer = find_llvm_symbolizer(args.llvm_path)
        if symbolizer:
            backend = "llvm-symbolizer (%s)" % symbolizer
            symmap = llvm_symbolize(symbolizer, dll, sorted(set(rvas)))
    if not symmap and pdb and rvas:
        backend = "built-in PDB reader"
        try:
            syms = PdbSymbols(pdb)
            for r in sorted(set(rvas)):
                s = syms.symbolize(r)
                symmap[r] = [(s, "")] if s else []
        except Exception as e:
            sys.stderr.write("warning: PDB parse failed: %s\n" % e)
    report["symbol_backend"] = backend

    def fmt_sym(rva):
        frames = symmap.get(rva) or []
        if not frames:
            return None
        parts = []
        for func, loc in frames:
            parts.append("%s [%s]" % (func, loc) if loc else func)
        return " <- inlined in ".join(parts)

    if crash_rva is not None:
        report["crash_symbol"] = fmt_sym(crash_rva)
    report["extra_rvas"] = {
        "0x%X" % int(r, 16): fmt_sym(int(r, 16)) for r in args.rva}
    report["stack"] = [
        {"addr": "0x%X" % f["stack_addr"],
         "target": "%s+0x%x" % (f["module"], f["rva"]),
         "symbol": (fmt_sym(f["rva"])
                    if f["module"].lower() == args.module.lower() else None)}
        for f in stack_frames]

    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print_report(report, dump, target_mod)

    if rvas and not symmap:
        sys.exit(2)


def print_report(report, dump, target_mod):
    width = 72
    print("=" * width)
    print("Minidump crash report: %s" % report["dump"])
    print("=" * width)
    if dump.system_info:
        print("OS: %s   modules: %d   threads: %d%s" % (
            dump.system_info.get("os", "?"),
            report["modules"], report["threads"],
            "   ** WINE/Proton (%s) **" % ", ".join(report["wine_modules"])
            if report.get("wine") else ""))
    if target_mod:
        print("Target module: %s  base=0x%X size=0x%X timestamp=0x%08X" % (
            target_mod["name"], target_mod["base"], target_mod["size"],
            target_mod["timestamp"]))
    exc = report.get("exception")
    if exc:
        print("\n-- Exception --")
        print("%s (%s) in thread 0x%x" % (exc["code"], exc["name"],
                                          exc["tid"]))
        loc = exc["address"]
        if exc["module"]:
            loc += "  =  %s+%s" % (exc["module"], exc["rva"])
        print("Faulting address: %s" % loc)
        if "access" in exc:
            print("Access violation: %s" % exc["access"])
        regs = exc["registers"]
        if regs:
            order = ["eax", "ebx", "ecx", "edx", "esi", "edi",
                     "ebp", "esp", "eip", "eflags"]
            print("Registers: " + "  ".join(
                "%s=%s" % (r, regs[r]) for r in order if r in regs))
        cxx = exc.get("cxx")
        if cxx:
            print("C++ exception: object=%s throw_info=%s" % (
                cxx["object"], cxx["throw_info"]))
            if cxx.get("thrown_type"):
                print("Thrown type: %s" % cxx["thrown_type"])
            elif cxx.get("types"):
                print("Thrown type (mangled): %s" % cxx["types"][0])
            if cxx.get("note"):
                print("(%s)" % cxx["note"])
    else:
        print("\n(no exception stream -- not a crash dump?)")
    sym = report.get("symbols", {})
    print("\n-- Symbols --")
    print("DLL: %s" % (sym.get("dll") or "(none)"))
    print("PDB: %s" % (sym.get("pdb") or "(none)"))
    if sym.get("guid"):
        print("RSDS GUID: %s" % sym["guid"])
    if report.get("symbol_backend"):
        print("Backend: %s" % report["symbol_backend"])
    if report.get("crash_symbol"):
        print("\n-- Crash location --")
        print(report["crash_symbol"])
    cl = report.get("crashes_log")
    if cl:
        print("\n-- crashes.log cross-check --")
        if "note_mismatch" in cl:
            print("WARNING: %s" % cl["note_mismatch"])
        if "os_info" in cl:
            print("OS Info: %s" % cl["os_info"])
        if "dll_version" in cl:
            print("DLL-Version: %s" % cl["dll_version"])
        if "file_offset" in cl:
            line = "Location (in file): %s+0x%x" % (cl.get("module", "?"),
                                                    cl["file_offset"])
            if "true_rva" in cl:
                line += "  ->  true RVA %s" % cl["true_rva"]
            if "wrapped_eip" in cl:
                line += ("  ->  eip=0x%X (outside any module; call through "
                         "NULL/garbage function pointer)" % cl["wrapped_eip"])
            print(line)
            if "note" in cl:
                print("(%s)" % cl["note"])
        if "largest_free_sub2g_k" in cl:
            # Distinguish Sub2G pressure from full-VA exhaustion: an LAA
            # process can have GBs free above 2 GB while Sub2G-bound
            # (non-LAA-safe) allocations still fail (issue #13262).
            sub2g = cl["largest_free_sub2g_k"]
            total = cl.get("largest_free_total_k")
            verdict = ""
            if sub2g < 4096:
                if total is not None and total >= 262144:
                    verdict = ("  ** likely Sub2G (below-2GB) exhaustion; "
                               "%d MB still free above 2GB **"
                               % (total // 1024))
                else:
                    verdict = ("  ** likely 32-bit address-space "
                               "exhaustion (OOM) **")
            print("Largest free block (Sub2G): %d KB%s" % (sub2g, verdict))
            if total is not None:
                base = cl.get("largest_free_total_base", 0)
                print("Largest free block (Total): %d KB at 0x%08X%s" % (
                    total, base,
                    "  (above 2GB line)" if base >= 0x80000000 else ""))
    sysmem = report.get("system_memory")
    if sysmem:
        print("\n-- System memory at crash (dump stream 21) --")
        print("RAM: %d MB (%d MB available)   System commit: "
              "%d / %d MB used (%d MB available)%s" % (
                  sysmem["phys_mb"], sysmem["phys_avail_mb"],
                  sysmem["commit_used_mb"], sysmem["commit_limit_mb"],
                  sysmem["commit_available_mb"],
                  "  ** SYSTEM COMMIT NEARLY EXHAUSTED **"
                  if sysmem["commit_available_mb"] < 512 else ""))
    vmc = report.get("vm_counters")
    if vmc:
        print("\n-- Process VM counters at crash (dump stream 22) --")
        parts = ["WorkingSet: %d MB" % vmc["ws_mb"],
                 "Commit (PagefileUsage): %d MB" % vmc["pagefile_mb"]]
        if "private_mb" in vmc:
            parts.append("Private: %d MB" % vmc["private_mb"])
        if "virtual_mb" in vmc:
            parts.append("VirtualSize: %d MB" % vmc["virtual_mb"])
        print("   ".join(parts))
    extra = report.get("extra_rvas", {})
    if extra:
        print("\n-- Extra RVAs --")
        for rva, s in extra.items():
            print("%s: %s" % (rva, s or "(no symbol)"))
    stack = report.get("stack", [])
    if stack:
        print("\n-- Stack scan (return-address candidates above ESP) --")
        print("Note: x86 has no reliable unwind info; entries are candidate")
        print("return addresses found by scanning, NOT a verified call chain.")
        for f in stack:
            line = "%s  %s" % (f["addr"], f["target"])
            if f["symbol"]:
                line += "  %s" % f["symbol"]
            print(line)
    print("=" * width)


if __name__ == "__main__":
    main()
