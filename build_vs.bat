@echo off
REM Build script for Community Patch DLL using CMake with Visual Studio 2019/2022

REM Parse command line arguments
set CONFIG=Debug
set VS_VERSION=2019
set TOOLCHAIN=msvc
set SDK_VERSION=7.0

:parse_args
if "%1"=="" goto end_parse_args
if /i "%1"=="release" set CONFIG=Release
if /i "%1"=="debug" set CONFIG=Debug
if /i "%1"=="vs2019" set VS_VERSION=2019
if /i "%1"=="vs2022" set VS_VERSION=2022
if /i "%1"=="msvc" set TOOLCHAIN=msvc
if /i "%1"=="clang" set TOOLCHAIN=clang
if /i "%1"=="sdk" set TOOLCHAIN=sdk
if /i "%1"=="sdk70" (
    set TOOLCHAIN=sdk
    set SDK_VERSION=7.0
)
if /i "%1"=="sdk70a" (
    set TOOLCHAIN=sdk
    set SDK_VERSION=7.0A
)
shift
goto parse_args
:end_parse_args

REM Create build directory based on toolchain
set BUILD_DIR=build_vs_%TOOLCHAIN%
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
cd %BUILD_DIR%

REM Configure CMake based on toolchain
if /i "%TOOLCHAIN%"=="msvc" (
    echo Configuring CMake for Visual Studio %VS_VERSION% with v90 toolset...
    
    if "%VS_VERSION%"=="2019" (
        cmake -G "Visual Studio 16 2019" -A Win32 -T v90 -DUSE_CLANG=OFF -DUSE_VS_IDE=ON ..
    ) else (
        cmake -G "Visual Studio 17 2022" -A Win32 -T v90 -DUSE_CLANG=OFF -DUSE_VS_IDE=ON ..
    )
) else if /i "%TOOLCHAIN%"=="clang" (
    echo Configuring CMake for Visual Studio %VS_VERSION% with VS2008 Clang toolchain...
    
    REM Check for VS2008 environment
    if not defined VS90COMNTOOLS (
        echo ERROR: Visual Studio 2008 environment not found.
        echo Please install Visual Studio 2008 or set VS90COMNTOOLS environment variable.
        echo Alternatively, use the SDK toolchain with: build_vs.bat sdk
        exit /b 1
    )
    
    if "%VS_VERSION%"=="2019" (
        cmake -G "Visual Studio 16 2019" -A Win32 -DCMAKE_TOOLCHAIN_FILE=../vs2008-clang-toolchain.cmake -DUSE_CLANG=ON -DUSE_VS_IDE=ON ..
    ) else (
        cmake -G "Visual Studio 17 2022" -A Win32 -DCMAKE_TOOLCHAIN_FILE=../vs2008-clang-toolchain.cmake -DUSE_CLANG=ON -DUSE_VS_IDE=ON ..
    )
) else if /i "%TOOLCHAIN%"=="sdk" (
    echo Configuring CMake for Visual Studio %VS_VERSION% with SDK %SDK_VERSION% Clang toolchain...
    
    if "%VS_VERSION%"=="2019" (
        cmake -G "Visual Studio 16 2019" -A Win32 -DCMAKE_TOOLCHAIN_FILE=../sdk70-clang-toolchain.cmake -DSDK_VERSION=%SDK_VERSION% -DUSE_CLANG=ON -DUSE_VS_IDE=ON ..
    ) else (
        cmake -G "Visual Studio 17 2022" -A Win32 -DCMAKE_TOOLCHAIN_FILE=../sdk70-clang-toolchain.cmake -DSDK_VERSION=%SDK_VERSION% -DUSE_CLANG=ON -DUSE_VS_IDE=ON ..
    )
)

REM Check configuration result
if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo Visual Studio solution has been generated in the build_vs directory.
echo You can now open the solution in Visual Studio or build it from the command line.

REM Build from command line if requested
if "%4"=="build" (
    echo Building %CONFIG% configuration...
    cmake --build . --config %CONFIG%
    
    if %ERRORLEVEL% NEQ 0 (
        echo Build failed with error code %ERRORLEVEL%
        exit /b %ERRORLEVEL%
    )
    
    echo Build completed successfully.
    
    REM Copy DLL to game directory if specified
    if defined GAME_DIR (
        echo Copying DLL to game directory: %GAME_DIR%
        copy /Y bin\%CONFIG%\CvGameCore_Expansion2.dll "%GAME_DIR%\"
        if exist bin\%CONFIG%\CvGameCore_Expansion2.pdb (
            copy /Y bin\%CONFIG%\CvGameCore_Expansion2.pdb "%GAME_DIR%\"
        )
    ) else (
        echo DLL built successfully in bin\%CONFIG%\
        echo To copy to game directory, set GAME_DIR environment variable before running this script.
    )
)

cd ..
exit /b 0