@echo On
REM Build script for Community Patch DLL using CMake with Clang and VS2008/SDK

REM Parse command line arguments
set CONFIG=Debug
set TOOLCHAIN=vs2008
set SDK_VERSION=7.0

:parse_args
if "%1"=="" goto end_parse_args
if /i "%1"=="release" set CONFIG=Release
if /i "%1"=="debug" set CONFIG=Debug
if /i "%1"=="vs2008" set TOOLCHAIN=vs2008
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

REM Create build directory
if not exist build mkdir build
cd build

REM Configure CMake with the appropriate toolchain
if /i "%TOOLCHAIN%"=="vs2008" (
    REM Check for VS2008 environment
    if not defined VS90COMNTOOLS (
        echo ERROR: Visual Studio 2008 environment not found.
        echo Please install Visual Studio 2008 or set VS90COMNTOOLS environment variable.
        echo Alternatively, use the SDK toolchain with: build.bat sdk
        exit /b 1
    )
    
    echo Configuring CMake with VS2008-Clang toolchain...
    call "%VS90COMNTOOLS%vsvars32.bat" >NUL
    cmake -G "NMake Makefiles" -DCMAKE_TOOLCHAIN_FILE=../vs2008-clang-toolchain.cmake -DCMAKE_BUILD_TYPE=%CONFIG% ..
) else (
    echo Configuring CMake with SDK70-Clang toolchain (SDK version: %SDK_VERSION%)...
    cmake -G "NMake Makefiles" -DCMAKE_TOOLCHAIN_FILE=../sdk70-clang-toolchain.cmake -DSDK_VERSION=%SDK_VERSION% -DCMAKE_BUILD_TYPE=%CONFIG% ..
)

REM Build the project
echo Building %CONFIG% configuration...
cmake --build . --config %CONFIG%

REM Check build result
if %ERRORLEVEL% NEQ 0 (
    echo Build failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo Build completed successfully.
cd ..

REM Copy DLL to game directory if specified
if defined GAME_DIR (
    echo Copying DLL to game directory: %GAME_DIR%
    copy /Y build\bin\%CONFIG%\CvGameCore_Expansion2.dll "%GAME_DIR%\"
    if exist build\bin\%CONFIG%\CvGameCore_Expansion2.pdb (
        copy /Y build\bin\%CONFIG%\CvGameCore_Expansion2.pdb "%GAME_DIR%\"
    )
) else (
    echo DLL built successfully in build\bin\%CONFIG%\
    echo To copy to game directory, set GAME_DIR environment variable before running this script.
)

echo Build process completed.
exit /b 0