# Minidump unwinder test harness (Windows)

Ground-truth tests for `scripts/analyze_minidump.py`'s x86 unwinder.

`host.exe` stands in for `CivilizationV.exe`: it loads a test
`CvGameCore_Expansion2.dll` and calls into it through an EXE-side dispatch
table. Every function records its own return address and stack slot before
the crash, and the DLL's unhandled-exception filter writes a minidump with
the VP Release handler's `MINIDUMP_TYPE` flags plus a `.truth.txt` file.

Both configurations use the official `build_vp_clang.py` codegen flags:
`release` (`/Ox /Ob2 -flto`, frame pointers omitted, LTO tail calls) and
`debug` (`/Od /Oy-`).

| Scenario | What it exercises |
|---|---|
| `stale` | write to NULL in an epilogue, under a deep chain of stale return addresses left by an earlier recursion inside an uninitialized 12 KB buffer |
| `nullcall` | call through a NULL callback in the DLL (`eip=0`) |
| `cxx` | unhandled C++ `std::bad_alloc` (0xE06D7363) through an import thunk |
| `exenull` | call through a NULL handler in the EXE (no DLL frames) |

```bat
build.bat release
build.bat debug
python check_truth.py
```

`build.bat` needs `clang-cl`/`lld-link` (`LLVM_BIN`) and an x86 MSVC
environment (`VCVARS32`, found with vswhere by default). Each case runs with
and without `--image host.exe`; without it, frames past the DLL may be guesses
but must then be reported `[unverified]`.
