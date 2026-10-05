@echo off
rem Build the minidump crash harness with the official build_vp_clang.py
rem codegen flags, then run every crash scenario to produce dumps.
rem Usage: build.bat release|debug
rem Needs clang-cl + lld-link (LLVM_BIN, default C:\Program Files\LLVM\bin)
rem and an x86 MSVC environment (VCVARS32, default: located with vswhere).
setlocal
set CFG=%1
if "%CFG%"=="" set CFG=release
if "%LLVM_BIN%"=="" set "LLVM_BIN=C:\Program Files\LLVM\bin"
if "%VCVARS32%"=="" (
  for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VCVARS32=%%i\VC\Auxiliary\Build\vcvars32.bat"
)
call "%VCVARS32%" >nul || exit /b 1
cd /d %~dp0
if not exist %CFG% mkdir %CFG%
cd %CFG%
if /i "%CFG%"=="release" (
  set CFLAGS=-m32 -msse3 /c /MD /GS /EHsc /fp:precise /Zc:wchar_t /Zi /FS /Ox /Ob2 /Zo -flto
  set LFLAGS=/MACHINE:x86 /DEBUG /LTCG /DYNAMICBASE /NXCOMPAT
) else (
  set CFLAGS=-m32 -msse3 /c /MD /GS /EHsc /fp:precise /Zc:wchar_t /Zi /FS /Od /Oy-
  set LFLAGS=/MACHINE:x86 /DEBUG /DYNAMICBASE /NXCOMPAT
)
"%LLVM_BIN%\clang-cl.exe" %CFLAGS% ..\dll.cpp /Fodll.obj || exit /b 1
"%LLVM_BIN%\clang-cl.exe" %CFLAGS% ..\host.cpp /Fohost.obj || exit /b 1
"%LLVM_BIN%\lld-link.exe" %LFLAGS% /DLL dll.obj /OUT:CvGameCore_Expansion2.dll /PDB:CvGameCore_Expansion2.pdb || exit /b 1
"%LLVM_BIN%\lld-link.exe" %LFLAGS% host.obj /OUT:host.exe /PDB:host.pdb || exit /b 1
for %%s in (stale nullcall cxx exenull) do .\host.exe %%s %CFG%
echo built and crashed %CFG%: dumps in %CD%
