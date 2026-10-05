#!/usr/bin/env python3
"""
Minidump crash analyzer for CvGameCore_Expansion2.dll.

Parses CvMiniDump_*.dmp files (including Wine-generated dumps that break the
python `minidump` library with a nonstandard 0xfff0 stream), pairs the crash
with the matching DLL/PDB, symbolizes the faulting address, unwinds the
crashed thread's stack, and prints a full crash report.

Stack unwinding (x86): the PDB's FrameData ("New FPO") records -- emitted by
clang-cl+lld and MSVC for every function, including /Oy (frame pointer
omitted) Release code -- give exact frames through the DLL, the same data
dbghelp/WinDbg and Breakpad use. Frames in modules without a PDB (the game
EXE, drivers, system DLLs) fall back to the EBP chain, then a stack scan.
Every frame is labelled with how it was found (context / cfi /
frame_pointer / call_to_null / scan), and guessed frames are checked
against the call instruction that precedes them -- including whether that
call leads into the function below, which rejects stale return addresses.
An eip=0 crash (call through a NULL function pointer) is resolved through
the return address the call left at [esp].

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
  --image FILE        Extra module image on disk, e.g. the exact
                      CivilizationV.exe / _DX11 / _Tablet the user ran
                      (repeatable). Optional: it lets guessed frames in
                      that module be verified against its code (Release
                      dumps do not contain module code; full-memory Debug
                      dumps do, and are verified from the dump), prints
                      IDA addresses for them, and translates crashes.log
                      file offsets for that module. The report warns when
                      frames could not be verified.
  --max-frames N      Unwind frame limit (default 64).
  --raw-scan          Also list every call-site-verified module address
                      above esp (diagnostic; includes stale frames).
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
  * Full-memory (Debug) dumps: memory-mapped and read through the Memory64
    list, so multi-GB dumps work and heap/code can be read at any address.
  * 32-bit addresses above 2 GB, which dbghelp stores sign-extended, are
    normalized (large-address-aware stacks and heaps live up there).

Exit codes: 0 ok, 1 bad input, 2 dump parsed but no symbols found.
"""

import argparse
import bisect
import json
import mmap
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

MINIDUMP_SIGNATURE = 0x504D444D  # 'MDMP'
STREAM_THREAD_LIST = 3
STREAM_MODULE_LIST = 4
STREAM_MEMORY_LIST = 5
STREAM_EXCEPTION = 6
STREAM_SYSTEM_INFO = 7
STREAM_MEMORY64_LIST = 9         # full-memory (Debug) dumps
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


def parse_x86_context(ctx):
    """Integer registers of an x86 CONTEXT (they start at offset 0x9C)."""
    if len(ctx) < 0x9C + 48:
        return {}
    names = ["edi", "esi", "ebx", "edx", "ecx", "eax",
             "ebp", "eip", "cs", "eflags", "esp", "ss"]
    return dict(zip(names, struct.unpack_from("<12I", ctx, 0x9C)))


# ---------------------------------------------------------------------------
# Minidump parsing (manual struct walk; Wine dumps have a nonstandard 0xfff0
# stream that trips strict parsers, so never validate stream types).
# ---------------------------------------------------------------------------

class Minidump(object):
    def __init__(self, path):
        self.path = path
        size = os.path.getsize(path)
        if size == 0:
            fail("%s: empty dump -- MiniDumpWriteDump itself failed, "
                 "usually because the process was out of memory (see the "
                 "matching crashes.log memory markers)" % path)
        if size < 32:
            fail("%s: too small to be a minidump (truncated write -- "
                 "likely OOM during dump capture)" % path)
        # Debug full-memory dumps run to several GB: map, don't read.
        with open(path, "rb") as f:
            self.data = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
        d = self.data
        sig, _ver, nstreams, dirrva = struct.unpack_from("<IIII", d, 0)
        if sig != MINIDUMP_SIGNATURE:
            fail("%s: bad signature 0x%08x (not MDMP)" % (path, sig))
        self.flags = struct.unpack_from("<Q", d, 24)[0]
        self.streams = {}
        for i in range(nstreams):
            stype, size, rva = struct.unpack_from("<III", d, dirrva + 12 * i)
            if stype:
                self.streams.setdefault(stype, (size, rva))
        self.system_info = self._parse_system_info()
        # dbghelp sign-extends 32-bit addresses >= 2 GB into the 64-bit
        # fields (0xAAED8FF4 -> 0xFFFFFFFFAAED8FF4). A large-address-aware
        # game under memory pressure lives up there, so mask them.
        self.addr_mask = 0xFFFFFFFF \
            if self.system_info.get("arch", 0) == 0 else (1 << 64) - 1
        self.modules = self._parse_modules()
        self.threads = self._parse_threads()
        self.memory_ranges = self._parse_memory_list()
        self.memory64_ranges = self._parse_memory64_list()
        self._index_memory()
        self.exception = self._parse_exception()
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
                "base": base & self.addr_mask,
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
            # MINIDUMP_THREAD: ThreadId, SuspendCount, PriorityClass,
            # Priority, Teb(8), then Stack = MINIDUMP_MEMORY_DESCRIPTOR
            # {StartOfMemoryRange(8) @24, DataSize @32, Rva @36} and
            # ThreadContext = {DataSize @40, Rva @44}.
            stack_start, stack_size, stack_rva, ctx_size, ctx_rva = \
                struct.unpack_from("<QIIII", e, 24)
            threads.append({
                "tid": tid,
                "stack_start": stack_start & self.addr_mask,
                "stack_size": stack_size,
                "stack_rva": stack_rva,
                "registers": parse_x86_context(
                    self.data[ctx_rva:ctx_rva + ctx_size]),
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
            ranges.append((start & self.addr_mask, size, rva))
        return ranges

    def _parse_memory64_list(self):
        """MINIDUMP_MEMORY64_LIST (full-memory dumps): NumberOfMemoryRanges,
        BaseRva, then {StartOfMemoryRange, DataSize} pairs whose data is
        stored back to back from BaseRva."""
        s = self._stream(STREAM_MEMORY64_LIST)
        if not s or len(s) < 16:
            return []
        n, off = struct.unpack_from("<QQ", s, 0)
        if 16 + 16 * n > len(s):
            return []
        ranges = []
        for i in range(n):
            start, size = struct.unpack_from("<QQ", s, 16 + 16 * i)
            ranges.append((start & self.addr_mask, size, off))
            off += size
        return ranges

    def _index_memory(self):
        # Full-memory dumps leave the thread list's stack Rva at 0 (the
        # bytes live in the Memory64 list); offset 0 is the header, never
        # data, so such descriptors are skipped.
        ranges = [(t["stack_start"], t["stack_size"], t["stack_rva"])
                  for t in self.threads]
        ranges += self.memory_ranges + self.memory64_ranges
        ranges = sorted(r for r in ranges
                        if r[1] and r[2] and r[2] + r[1] <= len(self.data))
        self._mem = ranges
        self._mem_starts = [r[0] for r in ranges]

    def read_va(self, addr, size):
        """Read bytes at a virtual address if captured in the dump (thread
        stacks, the memory list, or a full-memory dump's Memory64 list)."""
        i = bisect.bisect_right(self._mem_starts, addr) - 1
        # ranges may overlap (stacks also appear in the memory list)
        for j in range(i, max(i - 8, -1), -1):
            start, rsize, rva = self._mem[j]
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
        params = [p & self.addr_mask for p in
                  struct.unpack_from("<15Q", s, 8 + 28 + 4)[:nparams]]
        ctx_size, ctx_rva = struct.unpack_from("<II", s, 160)
        regs = parse_x86_context(self.data[ctx_rva:ctx_rva + ctx_size])
        return {
            "tid": tid,
            "code": code,
            "address": addr & self.addr_mask,
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
        if not thread["stack_rva"]:
            return self.read_va(thread["stack_start"],
                                thread["stack_size"]) or b""
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
    image_base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
    size_of_image = struct.unpack_from("<I", d, pe + 24 + 56)[0]
    secs_off = pe + 24 + optsize
    sections = parse_section_headers(d, secs_off, nsec)
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
            "image_base": image_base, "sections": sections, "guid": guid,
            "age": age, "pdb_name": pdb_name, "path": path, "data": d}


IMAGE_SCN_MEM_EXECUTE = 0x20000000


def parse_section_headers(d, secs_off, nsec):
    sections = []
    for i in range(nsec):
        s = secs_off + 40 * i
        name = d[s:s + 8].rstrip(b"\0").decode("ascii", errors="replace")
        vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", d, s + 8)
        chars = struct.unpack_from("<I", d, s + 36)[0]
        sections.append({"name": name, "va": va, "vsize": vsize,
                         "rawptr": rawptr, "rawsize": rawsize,
                         "exec": bool(chars & IMAGE_SCN_MEM_EXECUTE)})
    return sections


def pe_info_from_memory(dump, module):
    """Section table and ImageBase of a module whose PE headers were
    captured in the dump (full-memory dumps capture every image)."""
    hdr = dump.read_va(module["base"], 0x1000)
    if not hdr or hdr[:2] != b"MZ":
        return None
    pe = struct.unpack_from("<I", hdr, 0x3C)[0]
    if pe + 0x200 > len(hdr) or hdr[pe:pe + 4] != b"PE\0\0":
        return None
    nsec = struct.unpack_from("<H", hdr, pe + 6)[0]
    optsize = struct.unpack_from("<H", hdr, pe + 20)[0]
    secs_off = pe + 24 + optsize
    if secs_off + 40 * nsec > len(hdr):
        return None
    return {"image_base": struct.unpack_from("<I", hdr, pe + 24 + 28)[0],
            "sections": parse_section_headers(hdr, secs_off, nsec)}


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
        # Optional debug header: array of uint16 stream indices --
        # [0] FPO (old), [5] section headers, [9] NewFPO (FrameData).
        dbgoff = 64 + modinfo_sz + seccon_sz + secmap_sz + srcinfo_sz \
            + tsmap_sz + ec_sz
        dbghdr = dbi[dbgoff:dbgoff + dbghdr_sz]

        def dbg_stream(index):
            if len(dbghdr) < 2 * index + 2:
                return None
            s = struct.unpack_from("<H", dbghdr, 2 * index)[0]
            return None if s == 0xFFFF else s

        sect = self.msf.stream(dbg_stream(5))
        self.sec_rva = [struct.unpack_from("<I", sect, i * 40 + 12)[0]
                        for i in range(len(sect) // 40)]
        self._fpo_stream = dbg_stream(0)
        self._framedata_stream = dbg_stream(9)
        self._frame_index = None
        self._names = None
        self.procs = []    # (rva, length, name)
        self.publics = []  # (rva, name)
        self._parse_modules(dbi, modinfo_sz)
        self._parse_publics(self.msf.stream(symrec_stream))
        self.procs.sort()
        self.publics.sort()
        self._proc_starts = [p[0] for p in self.procs]

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

    def lookup(self, rva):
        """(name, start_rva, exact) for the function containing rva;
        exact=False means only the nearest public symbol below it."""
        i = bisect.bisect_right(self._proc_starts, rva) - 1
        for j in range(i, max(i - 8, -1), -1):
            prva, plen, name = self.procs[j]
            if prva <= rva < prva + plen:
                return name, prva, True
        best = None
        for prva, name in self.publics:
            if prva <= rva and (best is None or prva > best[0]):
                best = (prva, name)
        if best:
            return best[1], best[0], False
        return None

    def symbolize(self, rva, offset_rva=None):
        """'name+0xoff'. offset_rva lets a caller frame be looked up at
        ret-1 (inside the call) but displayed at the return address."""
        hit = self.lookup(rva)
        if not hit:
            return None
        name, start, exact = hit
        off = (rva if offset_rva is None else offset_rva) - start
        return "%s+0x%x%s" % (name, off, "" if exact else " (nearest public)")

    # -- x86 unwind data (what dbghelp/WinDbg and Breakpad STACK WIN use) --

    def _names_buffer(self):
        """The PDB string table (/names), which FrameData program strings
        index into. Located through the info stream's named-stream map."""
        info = self.msf.stream(1)
        off = 28  # Version, Signature, Age, Guid
        nbuf = struct.unpack_from("<I", info, off)[0]
        strbuf = info[off + 4:off + 4 + nbuf]
        off += 4 + nbuf
        _size, cap = struct.unpack_from("<II", info, off)
        off += 8
        nwords = struct.unpack_from("<I", info, off)[0]
        present = struct.unpack_from("<%dI" % nwords, info, off + 4)
        off += 4 + 4 * nwords
        ndeleted = struct.unpack_from("<I", info, off)[0]
        off += 4 + 4 * ndeleted
        for i in range(cap):
            if i // 32 < nwords and (present[i // 32] >> (i % 32)) & 1:
                key, stream = struct.unpack_from("<II", info, off)
                off += 8
                if strbuf[key:strbuf.index(b"\0", key)] == b"/names":
                    names = self.msf.stream(stream)
                    sig, _ver, size = struct.unpack_from("<III", names, 0)
                    if sig != 0xEFFEEFFE:
                        return None
                    return names[12:12 + size]
        return None

    def _load_frame_index(self):
        recs = []
        if self._framedata_stream is not None:
            data = self.msf.stream(self._framedata_stream)
            start = 4 if len(data) % 32 else 0  # optional RelocPtr header
            self._names = self._names_buffer()
            for o in range(start, len(data) - 31, 32):
                (rva, size, locals_, params, _maxstack, func, prolog,
                 saved, flags) = struct.unpack_from("<IIIIIIHHI", data, o)
                recs.append((rva, size, 0, (locals_, params, saved, prolog,
                                            func, flags)))
        if self._fpo_stream is not None:
            data = self.msf.stream(self._fpo_stream)
            for o in range(0, len(data) - 15, 16):
                rva, size, locals_dw, params_dw, attr = \
                    struct.unpack_from("<IIIHH", data, o)
                # FPO_DATA bitfields: cbProlog:8 cbRegs:3 fHasSEH:1
                # fUseBP:1 reserved:1 cbFrame:2
                recs.append((rva, size, 1, (locals_dw * 4, params_dw * 4,
                                            ((attr >> 8) & 7) * 4,
                                            attr & 0xFF,
                                            bool(attr & 0x1000), attr >> 14)))
        recs.sort()
        self._frame_index = ([r[0] for r in recs], recs)

    def has_frame_data(self):
        if self._frame_index is None:
            self._load_frame_index()
        return bool(self._frame_index[1])

    def frame_info(self, rva):
        """Unwind record covering rva: FrameData (with its program string)
        preferred over old FPO. Per function there is one record per
        prolog step, each running to the function end, so the record with
        the greatest start <= rva is the one for this instruction."""
        if self._frame_index is None:
            self._load_frame_index()
        starts, recs = self._frame_index
        i = bisect.bisect_right(starts, rva) - 1
        best = None
        for j in range(i, max(i - 64, -1), -1):
            start, size, kind, v = recs[j]
            if start <= rva < start + size and (
                    best is None or (kind, -start) < (best[2], -best[0])):
                best = recs[j]
        if not best:
            return None
        start, size, kind, v = best
        if kind == 0:
            locals_, params, saved, prolog, func, flags = v
            prog = None
            if self._names is not None and func < len(self._names):
                end = self._names.find(b"\0", func)
                prog = self._names[func:end].decode("ascii", "replace")
            return {"type": "framedata", "start": start, "size": size,
                    "locals": locals_, "params": params, "saved_regs": saved,
                    "prolog": prolog, "program": prog or None}
        locals_, params, saved, prolog, uses_bp, frame_type = v
        return {"type": "fpo", "start": start, "size": size,
                "locals": locals_, "params": params, "saved_regs": saved,
                "prolog": prolog, "uses_bp": uses_bp,
                "frame_type": frame_type, "program": None}


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


def llvm_symbolize(symbolizer, dll, rvas, pdb=None, pdb_name=None):
    """Symbolize RVAs; returns {rva: [(func, file:line), ...]} incl. inlines.

    llvm-symbolizer only takes the DLL: it loads the PDB named by the DLL's
    RSDS record from the DLL's directory, else from the RSDS absolute path,
    without checking the GUID -- a stale build at that path gives silently
    wrong symbols. When the matched PDB is not that file, stage both in a
    temporary directory under the names it looks for."""
    if not rvas:
        return {}
    want = os.path.join(os.path.dirname(os.path.abspath(dll)),
                        os.path.basename((pdb_name or "").replace("\\", "/")))
    if pdb and pdb_name and os.path.normcase(os.path.abspath(pdb)) != \
            os.path.normcase(want):
        with tempfile.TemporaryDirectory() as tmp:
            staged = os.path.join(tmp, os.path.basename(dll))
            for src, dst in ((dll, staged),
                             (pdb, os.path.join(tmp, os.path.basename(want)))):
                try:
                    os.link(src, dst)
                except OSError:
                    shutil.copyfile(src, dst)
            return llvm_symbolize(symbolizer, staged, rvas)
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
# Call-site verification of return addresses
# ---------------------------------------------------------------------------

def _modrm_operand_len(b, i):
    """Bytes taken by the ModRM byte at b[i] plus its SIB/displacement."""
    modrm = b[i]
    mod, rm = modrm >> 6, modrm & 7
    n = 1
    if mod == 3:
        return n
    if rm == 4:
        if i + 1 >= len(b):
            return -1
        n += 1
        if mod == 0 and (b[i + 1] & 7) == 5:
            n += 4
    elif mod == 0 and rm == 5:
        n += 4
    if mod == 1:
        n += 1
    elif mod == 2:
        n += 4
    return n


class CodeView(object):
    """One module's code, from its image on disk or from dump memory.

    sections: parsed section headers; read(rva, n) -> bytes or None.
    Calling it checks a candidate return address: True when rva lies in an
    executable section directly after a call instruction -- `E8 rel32`
    whose target is executable code in the same module, or an `FF /2`
    indirect call whose full encoding (ModRM/SIB/displacement) ends exactly
    at rva -- False when it does not, None when the bytes are unavailable.
    """

    def __init__(self, sections, read):
        self.exec_secs = [(s["va"], max(s["vsize"], s["rawsize"]))
                          for s in sections if s["exec"]]
        self.read = read

    def in_exec(self, rva):
        return any(va <= rva < va + size for va, size in self.exec_secs)

    def call_before(self, rva):
        """("direct", target_rva), ("indirect", None), None (no call
        instruction ends at rva) or "unknown" (bytes unavailable)."""
        if not self.in_exec(rva) or rva < 7:
            return None
        b = self.read(rva - 7, 7)
        if not b or len(b) < 7:
            return "unknown"
        if b[2] == 0xE8:
            target = (rva + struct.unpack_from("<i", b, 3)[0]) & 0xFFFFFFFF
            if self.in_exec(target):
                target = self.follow_thunk(target)
                if target is None:
                    return ("indirect", None)  # import thunk
                return ("direct", target)
        for op in range(0, 6):
            if b[op] == 0xFF and (b[op + 1] >> 3) & 7 == 2 and \
                    _modrm_operand_len(b, op + 1) == 6 - op:
                return ("indirect", None)
        return None

    def follow_thunk(self, rva):
        """Resolve jump thunks: `jmp rel32` (incremental linking) to the
        real function; None for an import thunk `jmp [__imp_X]`, whose
        target lives in another module."""
        b = self.read(rva, 6)
        if b and len(b) >= 5 and b[0] == 0xE9:
            target = (rva + 5 + struct.unpack_from("<i", b, 1)[0]) \
                & 0xFFFFFFFF
            if self.in_exec(target):
                return target
        if b and len(b) == 6 and b[0] == 0xFF and b[1] == 0x25:
            return None
        return rva

    def __call__(self, rva):
        kind = self.call_before(rva)
        if kind == "unknown":
            return None
        return kind is not None


def make_return_address_checker(dll_path):
    """CodeView backed by a module image on disk."""
    pe = pe_info(dll_path)
    data = pe["data"]

    def read(rva, n):
        off = rva_to_off(pe["sections"], rva)
        return None if off is None else data[off:off + n]

    return CodeView(pe["sections"], read)


def make_memory_code_checker(dump, module):
    """CodeView backed by the dump itself (full-memory dumps capture every
    image, so even the game EXE and system DLLs can be verified)."""
    info = pe_info_from_memory(dump, module)
    if not info:
        return None
    base = module["base"]
    return CodeView(info["sections"],
                    lambda rva, n: dump.read_va(base + rva, n))


# ---------------------------------------------------------------------------
# x86 unwinder
#
# The PDB's FrameData ("New FPO") records are the x86 unwind information:
# MSVC and clang-cl+lld emit one per prolog step of every function, each with
# a postfix program that recovers the caller's $eip/$esp/$ebp (what
# dbghelp/WinDbg and Breakpad "STACK WIN 4" use). Semantics follow Breakpad
# and rust-minidump. Each frame records how it was found:
#   context        registers from the exception record
#   cfi            PDB FrameData/FPO unwind record (exact)
#   frame_pointer  EBP chain, return address verified as a call site
#   call_to_null   eip outside every module: [esp] is the return address
#                  pushed by the call that jumped there (call-site verified)
#   scan           first plausible return address above esp (a guess)
# ---------------------------------------------------------------------------

def eval_frame_program(program, variables, read_u32):
    """Evaluate a FrameData postfix program; returns the variable map or
    None. Operators: + - * / % @(align) ^(deref) =(assign)."""
    stack = []

    def val(x):
        if isinstance(x, int):
            return x
        return variables.get(x)

    tokens = []
    for t in program.split():
        # some toolchains emit "=$eip" with no separating space
        if t.startswith("=") and len(t) > 1:
            tokens += ["=", t[1:]]
        else:
            tokens.append(t)
    for t in tokens:
        if t in ("+", "-", "*", "/", "%", "@"):
            if len(stack) < 2:
                return None
            b, a = val(stack.pop()), val(stack.pop())
            if a is None or b is None:
                return None
            if t == "+":
                r = a + b
            elif t == "-":
                r = a - b
            elif t == "*":
                r = a * b
            elif t in ("/", "%"):
                if b == 0:
                    return None
                r = a // b if t == "/" else a % b
            else:
                if b == 0 or b & (b - 1):
                    return None
                r = a & ~(b - 1)
            stack.append(r & 0xFFFFFFFF)
        elif t == "^":
            if not stack:
                return None
            a = val(stack.pop())
            v = None if a is None else read_u32(a)
            if v is None:
                return None
            stack.append(v)
        elif t == "=":
            if len(stack) < 2:
                return None
            v, name = stack.pop(), stack.pop()
            if not isinstance(name, str):
                return None
            if v == ".undef":
                variables.pop(name, None)
                continue
            v = val(v)
            if v is None:
                return None
            variables[name] = v
        elif t[0] in "$." or not re.match(r"^-?\d+$", t):
            stack.append(t)
        else:
            stack.append(int(t) & 0xFFFFFFFF)
    return variables


class Unwinder(object):
    """Walks one thread's stack from an x86 register context.

    symbols: lowercase module name -> PdbSymbols (frame info + names)
    checkers: lowercase module name -> code checker (see make_code_checker)
    """

    CALLEE_SAVED = ("ebp", "ebx", "esi", "edi")

    def __init__(self, dump, thread, symbols, checkers):
        self.dump = dump
        self.symbols = symbols
        self.checkers = checkers
        self.stack_lo = thread["stack_start"]
        self.stack_hi = thread["stack_start"] + thread["stack_size"]

    def read_u32(self, addr):
        if not 0 <= addr <= 0xFFFFFFFC:
            return None
        return self.dump.read_u32(addr)

    def return_address_status(self, value):
        """True (verified call site), None (in a module, code unavailable)
        or False (not a return address)."""
        m = self.dump.module_for(value)
        if not m:
            return False
        check = self.checkers.get(m["name"].lower())
        if check is None:
            return None
        return check(value - m["base"])

    def plausible(self, value):
        return self.return_address_status(value) is not False

    def function_start(self, frame):
        """VA of the function a frame is executing, from the PDB."""
        if frame["module"] is None:
            return None
        syms = self.symbols.get(frame["module"].lower())
        if syms is None:
            return None
        lookup = frame["rva"] - (0 if frame["trust"] == "context" else 1)
        hit = syms.lookup(lookup)
        if not hit or not hit[2]:
            return None
        return frame["eip"] - frame["rva"] + hit[1]

    def call_consistency(self, ret, callee):
        """Does the call that pushed `ret` lead to the callee's function?

        "match" -- a direct call to the callee's function start (PDB)
        "weak"  -- no PDB for the callee: the direct call lands in the
                   callee's module at most 128 KB before its eip
        "mismatch" -- a direct call to somewhere else: `ret` is a stale
                   return address, unless the callee was reached by a
                   tail call
        None    -- cannot tell (indirect call, or no code bytes)
        """
        if callee["module"] is None:
            return None
        m = self.dump.module_for(ret)
        code = self.checkers.get(m["name"].lower()) if m else None
        if code is None:
            return None
        kind = code.call_before(ret - m["base"])
        if not isinstance(kind, tuple) or kind[0] != "direct":
            return None
        target = m["base"] + kind[1]
        start = self.function_start(callee)
        if start is not None:
            return "match" if target == start else "mismatch"
        tm = self.dump.module_for(target)
        if tm is None or tm["name"] != callee["module"]:
            return "mismatch"
        if target <= callee["eip"] < target + 0x20000:
            return "weak"
        return "mismatch"

    def _frame(self, regs, trust, info=None, note=None):
        eip = regs["eip"]
        m = self.dump.module_for(eip)
        status = self.return_address_status(eip)
        return {"regs": regs, "trust": trust, "info": info, "note": note,
                "eip": eip, "esp": regs["esp"],
                "module": m["name"] if m else None,
                "rva": eip - m["base"] if m else None,
                "verified": status is True,
                "code_missing": status is None}

    def walk(self, ctx, max_frames=64):
        regs = {k: ctx[k] for k in ("eip", "esp") + self.CALLEE_SAVED
                if k in ctx}
        frames = [self._frame(regs, "context")]
        while len(frames) < max_frames:
            callee = frames[-1]
            caller = None
            if callee["trust"] == "context" and callee["module"] is None:
                caller = self._by_call_to_null(callee)
            if caller is None and callee["module"] is not None:
                caller = self._by_frame_info(callee, frames)
            if caller is None:
                caller = self._by_frame_pointer(callee)
            if caller is None:
                caller = self._by_scan(callee)
            if caller is None or caller["esp"] <= callee["esp"] \
                    or caller["eip"] == 0 or caller["module"] is None:
                break
            # does the call before this return address lead to the callee?
            caller["link"] = self.call_consistency(caller["eip"], callee)
            frames.append(caller)
            if caller["esp"] >= self.stack_hi:
                break
        return frames

    def _by_call_to_null(self, callee):
        """eip outside every module (typically 0: a call through a NULL
        function pointer or vtable slot). The call pushed its return
        address, which is still at [esp]."""
        esp = callee["esp"]
        ret = self.read_u32(esp)
        if ret is None or self.return_address_status(ret) is False:
            return None
        regs = {"eip": ret, "esp": esp + 4}
        for r in self.CALLEE_SAVED:
            if r in callee["regs"]:
                regs[r] = callee["regs"][r]
        return self._frame(regs, "call_to_null")

    def _by_frame_info(self, callee, frames):
        syms = self.symbols.get(callee["module"].lower())
        if syms is None or not syms.has_frame_data():
            return None
        # A return address points after the call; look the caller up
        # inside the call (ret-1) so a call ending a function resolves to
        # that function, not the next one.
        lookup = callee["rva"] - (0 if callee["trust"] == "context" else 1)
        info = syms.frame_info(lookup)
        if info is None:
            return None
        grand = frames[-2] if len(frames) >= 2 else None
        grand_params = grand["info"]["params"] \
            if grand and grand["info"] else 0
        base = info["locals"] + info["saved_regs"]
        sizes = [base + grand_params]
        if grand_params:
            # clang reserves outgoing-argument space inside the frame
            # (sub esp, N) instead of pushing, so the grand-callee's
            # parameters are already counted in cbLocals.
            sizes.append(base)
        results = [r for r in (self._eval_frame_info(callee, info, s)
                               for s in sizes) if r is not None]
        for r in results:
            if r["verified"] and \
                    self.call_consistency(r["eip"], callee) != "mismatch":
                return r
        # The record describes the function body after its prolog. At the
        # faulting instruction esp can differ (epilogue already popped the
        # saved registers; arguments pushed for a call), and a caller of a
        # frame without unwind info (call_to_null, EXE) pushed an unknown
        # number of argument bytes. Then the computed slot holds a stale
        # value: look nearby for the slot whose call leads to this function.
        alt = self._consistent_slot(callee, info, callee["esp"],
                                    callee["esp"] + max(sizes) + 64)
        if alt is not None:
            return alt
        for r in results:
            if self.plausible(r["eip"]) and \
                    self.call_consistency(r["eip"], callee) != "mismatch":
                return r
        # a mismatching but valid call site may still be right after a
        # tail call; keep the unwind record's answer
        if results and results[0]["verified"]:
            return results[0]
        return None

    def _consistent_slot(self, callee, info, lo, hi):
        search = callee["esp"] + info["locals"] + info["saved_regs"]
        slots = sorted(range(lo & ~3, hi, 4), key=lambda a: abs(a - search))
        for addr in slots:
            v = self.read_u32(addr)
            if v is None or self.return_address_status(v) is not True:
                continue
            if self.call_consistency(v, callee) in ("match", "weak"):
                r = self._eval_frame_info(callee, info, None, search=addr)
                if r is not None and r["eip"] == v:
                    r["note"] = "return-address slot corrected"
                    return r
        return None

    def _eval_frame_info(self, callee, info, frame_size, search=None):
        regs = callee["regs"]
        esp = regs["esp"]
        if frame_size is None:
            frame_size = search - esp
        if info["type"] == "framedata" and info["program"]:
            prog = info["program"]
            if search is not None:
                pass
            elif "@" in prog:
                # stack realigned: the return address sits above saved ebp
                if "ebp" not in regs:
                    return None
                search = regs["ebp"] + 4
            else:
                search = esp + frame_size
            variables = {"$" + r: regs[r]
                         for r in ("esp",) + self.CALLEE_SAVED if r in regs}
            variables.update({
                ".cbParams": info["params"],
                ".cbCalleeParams": max(0, frame_size - info["locals"]
                                       - info["saved_regs"]),
                ".cbSavedRegs": info["saved_regs"],
                ".cbLocals": info["locals"],
                ".raSearch": search & 0xFFFFFFFF,
                ".raSearchStart": search & 0xFFFFFFFF,
            })
            out = eval_frame_program(prog, variables, self.read_u32)
            if out is None or "$eip" not in out or "$esp" not in out:
                return None
            new = {"eip": out["$eip"], "esp": out["$esp"]}
            for r in self.CALLEE_SAVED:
                if "$" + r in out:
                    new[r] = out["$" + r]
            return self._frame(new, "cfi", info)
        # old FPO record (or FrameData without a program string)
        ra = esp + frame_size
        eip = self.read_u32(ra)
        if eip is not None and eip == callee["eip"] \
                and callee["trust"] == "context":
            ra += 4  # leftover return address of a FPO system call
            eip = self.read_u32(ra)
        if eip is None:
            return None
        new = {"eip": eip, "esp": ra + 4}
        if info.get("uses_bp"):
            ebp = self.read_u32(esp + (frame_size - info["locals"]
                                       - info["saved_regs"])
                                + info["saved_regs"] - 8)
            if ebp is not None:
                new["ebp"] = ebp
        else:
            for r in ("ebp", "ebx"):
                if r in regs:
                    new[r] = regs[r]
        return self._frame(new, "cfi", info)

    def _by_frame_pointer(self, callee):
        ebp = callee["regs"].get("ebp")
        if ebp is None or ebp % 4 or \
                not callee["esp"] <= ebp < self.stack_hi - 8:
            return None
        eip = self.read_u32(ebp + 4)
        new_ebp = self.read_u32(ebp)
        if eip is None or new_ebp is None:
            return None
        # In FPO code ebp is a general register (or an outer frame's
        # pointer): accept a link only when its return address is a
        # verified call site, or -- when the code is unavailable -- the
        # chain continues sanely; never when the call leads elsewhere.
        status = self.return_address_status(eip)
        if status is False or \
                self.call_consistency(eip, callee) == "mismatch":
            return None
        chained = ebp < new_ebp < self.stack_hi or new_ebp == 0
        if status is None and not chained:
            return None
        regs = {"eip": eip, "esp": ebp + 8}
        if ebp < new_ebp < self.stack_hi:
            regs["ebp"] = new_ebp
        return self._frame(regs, "frame_pointer")

    def _by_scan(self, callee):
        """First return address above esp, preferring (1) verified call
        sites whose call can lead to the callee, then (2) any candidate
        not proven to belong to another function, then (3) anything in a
        module. Each pass skips values left by unrelated earlier calls
        that the previous pass could rule out."""
        esp = callee["esp"]
        words = 160 if callee["trust"] in ("context", "cfi") else 40
        seen = []
        for i in range(words):
            v = self.read_u32(esp + 4 * i)
            if v is None:
                break
            if v == callee["eip"] and callee["trust"] == "context":
                continue
            status = self.return_address_status(v)
            if status is not False:
                seen.append((i, v, status,
                             self.call_consistency(v, callee)))
        passes = (lambda s, c: s is True and c != "mismatch",
                  lambda s, c: c != "mismatch",
                  lambda s, c: True)
        for accept in passes:
            for i, v, status, cons in seen:
                if not accept(status, cons):
                    continue
                addr = esp + 4 * i
                regs = {"eip": v, "esp": addr + 4}
                if i > 0:
                    bp = self.read_u32(addr - 4)
                    if bp is not None and addr < bp < self.stack_hi and \
                            bp - addr <= 128 * 1024:
                        regs["ebp"] = bp
                return self._frame(regs, "scan")
        return None


def scan_stack(dump, thread, from_addr, max_frames, checkers=None):
    """Raw scan: every module address above from_addr that passes the
    module's call-site check (when one is available). Diagnostic only --
    stale return addresses left by earlier, deeper calls are reported too;
    use Unwinder for the call chain."""
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
        if check and check(v - m["base"]) is False:
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
    ap.add_argument("--max-frames", type=int, default=64)
    ap.add_argument("--raw-scan", action="store_true",
                    help="also list every call-site-verified module address "
                         "above esp (diagnostic; includes stale frames)")
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
        # beside the DLL, then the build path recorded in it -- only with a
        # matching GUID (a stale build there gives silently wrong symbols)
        for cand in (os.path.join(os.path.dirname(dll), os.path.basename(
                         pe["pdb_name"].replace("\\", "/"))),
                     pe["pdb_name"]):
            if os.path.isfile(cand) and pdb_guid(cand) == (pe["guid"],
                                                          pe["age"]):
                pdb = cand
                break
        if not pdb:
            sys.stderr.write("warning: no PDB matching %s's GUID found; "
                             "pass --pdb or --symbols\n" % dll)
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
    raw_scan = []
    image_bases = {}
    pdb_syms = None
    if pdb:
        try:
            pdb_syms = PdbSymbols(pdb)
        except Exception as e:
            sys.stderr.write("warning: PDB parse failed: %s\n" % e)
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
                    image_bases[args.module.lower()] = pe["image_base"]
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
                image_bases[name] = info["image_base"]
            # Full-memory dumps carry every image: verify the rest in place.
            # (Not their ImageBase: the loader rewrites it to the load
            # address, so IDA addresses come only from --image files.)
            for m in dump.modules:
                name = m["name"].lower()
                if name not in checkers:
                    checker = make_memory_code_checker(dump, m)
                    if checker:
                        checkers[name] = checker
            symbols = {}
            if pdb_syms is not None:
                symbols[args.module.lower()] = pdb_syms
            report["unwind_info"] = bool(pdb_syms is not None and
                                         pdb_syms.has_frame_data())
            unwinder = Unwinder(dump, thread, symbols, checkers)
            stack_frames = unwinder.walk(exc["registers"], args.max_frames)
            for i, f in enumerate(stack_frames):
                if f["module"] and f["module"].lower() == \
                        args.module.lower() and i > 0:
                    rvas.append(f["rva"] - 1)  # inside the call
            if args.raw_scan:
                raw_scan = scan_stack(dump, thread, esp, args.max_frames,
                                      checkers)

    symmap = {}
    backend = None
    if dll and pdb and rvas:
        symbolizer = find_llvm_symbolizer(args.llvm_path)
        if symbolizer:
            backend = "llvm-symbolizer (%s)" % symbolizer
            symmap = llvm_symbolize(symbolizer, dll, sorted(set(rvas)),
                                    pdb, pe["pdb_name"] if pe else None)
    if not symmap and pdb_syms is not None and rvas:
        backend = "built-in PDB reader"
        for r in sorted(set(rvas)):
            s = pdb_syms.symbolize(r)
            symmap[r] = [(s, "")] if s else []
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
    target = args.module.lower()
    report["stack"] = []
    for i, f in enumerate(stack_frames):
        entry = {"frame": i, "trust": f["trust"],
                 "eip": "0x%X" % f["eip"], "esp": "0x%X" % f["esp"],
                 "target": ("%s+0x%x" % (f["module"], f["rva"])
                            if f["module"] else "0x%X" % f["eip"])}
        name = (f["module"] or "").lower()
        if name in image_bases and name != target:
            entry["image_va"] = "0x%X" % (image_bases[name] + f["rva"])
        if name == target:
            lookup = f["rva"] - (1 if i else 0)
            if pdb_syms is not None:
                entry["function"] = pdb_syms.symbolize(lookup, f["rva"])
            loc = fmt_sym(lookup)
            if loc and loc != entry.get("function"):
                entry["location"] = loc
        if f.get("info") and f["info"].get("type") == "fpo":
            entry["note"] = "old FPO record"
        if f.get("note"):
            entry["note"] = f["note"]
        if f.get("link"):
            entry["link"] = f["link"]
        if i and f["trust"] != "cfi" and f["code_missing"]:
            entry["unverified"] = True
        report["stack"].append(entry)
    # Frames guessed (frame pointer / scan) in a module whose code is not
    # in the dump could not be checked against a call instruction.
    report["unverified_modules"] = sorted(set(
        f["module"] for i, f in enumerate(stack_frames)
        if i and f["trust"] != "cfi" and f["code_missing"]))
    report["raw_scan"] = [
        {"addr": "0x%X" % f["stack_addr"],
         "target": "%s+0x%x" % (f["module"], f["rva"]),
         "symbol": (pdb_syms.symbolize(f["rva"] - 1, f["rva"])
                    if pdb_syms is not None and
                    f["module"].lower() == target else None)}
        for f in raw_scan]

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
        print("\n-- Call stack (crashed thread) --")
        if not report.get("unwind_info"):
            print("(no PDB unwind records for %s: its frames fall back to "
                  "the frame pointer or a scan)" % (
                      target_mod["name"] if target_mod else "the module"))
        for f in stack:
            line = "#%02d %-13s %s" % (f["frame"], f["trust"], f["target"])
            if f.get("image_va"):
                line += " (IDA 0x%s)" % f["image_va"][2:]
            if f.get("function"):
                line += "  %s" % f["function"]
            if f.get("unverified"):
                line += "  [unverified]"
            if f.get("link") in ("match", "weak") and f["trust"] == "scan":
                line += "  [calls into #%02d]" % (f["frame"] - 1)
            elif f.get("link") == "mismatch":
                line += "  [call leads elsewhere: tail call or stale]"
            if f.get("note"):
                line += "  (%s)" % f["note"]
            print(line)
            if f.get("location"):
                print("    %s" % f["location"])
        print("Trust: context = exception registers; cfi = PDB unwind "
              "record (exact);\nframe_pointer = EBP chain; call_to_null = "
              "[esp] after a call to an invalid\naddress; scan = first "
              "plausible return address (a guess -- frames after a\nscan "
              "may be stale). Guessed frames are checked against the call "
              "instruction\nbefore them when the module's code is available.")
        unverified = report.get("unverified_modules") or []
        if unverified:
            print("WARNING: [unverified] frames in %s: the dump does not "
                  "contain that code,\nso a stale value that merely points "
                  "into the module could not be ruled out." %
                  ", ".join(unverified))
            exes = [m for m in unverified if m.lower().endswith(".exe")]
            if exes:
                print("Pass --image <path to %s> (the exact build the user "
                      "ran) to verify\nthose frames; frames with PDB unwind "
                      "records do not depend on it." % exes[0])
    raw = report.get("raw_scan", [])
    if raw:
        print("\n-- Raw stack scan (diagnostic; includes stale frames) --")
        for f in raw:
            line = "%s  %s" % (f["addr"], f["target"])
            if f["symbol"]:
                line += "  %s" % f["symbol"]
            print(line)
    print("=" * width)


if __name__ == "__main__":
    main()
