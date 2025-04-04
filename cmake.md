# Community Patch DLL CMake Build System

This build system allows building the Community Patch DLL using CMake with multiple toolchain options while maintaining compatibility with the original game binary.

## Requirements

### For VS2008 Toolchain with Clang
- Visual Studio 2008 (for VS90COMNTOOLS environment variable and libraries)
- CMake 3.15 or higher
- Clang (clang-cl.exe and lld-link.exe must be in PATH)

### For SDK 7.0/7.0A Toolchain with Clang
- Windows SDK 7.0 or 7.0A
- Visual Studio 9.0 includes
- CMake 3.15 or higher
- Clang (clang-cl.exe and lld-link.exe must be in PATH)

### For Visual Studio 2019/2022 with v90 Toolset
- Visual Studio 2019 or 2022
- Visual Studio 2008 toolset (v90) installed via Visual Studio Installer
- CMake 3.15 or higher

## Build Instructions

### Using the Clang Build Script

For Clang-based builds (with VS2008 or SDK), use the provided build script:

```
build.bat [debug|release] [vs2008|sdk|sdk70|sdk70a]
```

Parameters:
- First parameter: Build configuration (`debug` or `release`, default is `debug`)
- Second parameter: Toolchain to use:
  - `vs2008`: Use Visual Studio 2008 environment (default)
  - `sdk` or `sdk70`: Use Windows SDK 7.0
  - `sdk70a`: Use Windows SDK 7.0A

Examples:
```
build.bat                  # Debug build with VS2008 toolchain
build.bat release          # Release build with VS2008 toolchain
build.bat debug sdk        # Debug build with SDK 7.0 toolchain
build.bat release sdk70a   # Release build with SDK 7.0A toolchain
```

### Using the Visual Studio Build Script

For Visual Studio 2019/2022 builds with any toolchain, use the VS-specific build script:

```
build_vs.bat [debug|release] [vs2019|vs2022] [msvc|clang|sdk|sdk70|sdk70a] [build]
```

Parameters:
- First parameter: Build configuration (`debug` or `release`, default is `debug`)
- Second parameter: Visual Studio version (`vs2019` or `vs2022`, default is `vs2019`)
- Third parameter: Toolchain to use:
  - `msvc`: Use Visual Studio with v90 toolset (default)
  - `clang`: Use Visual Studio with VS2008 Clang toolchain
  - `sdk` or `sdk70`: Use Visual Studio with SDK 7.0 Clang toolchain
  - `sdk70a`: Use Visual Studio with SDK 7.0A Clang toolchain
- Fourth parameter: Optional `build` flag to build from command line instead of just generating the solution

Examples:
```
build_vs.bat                       # Generate VS2019 solution with v90 toolset
build_vs.bat release vs2022 msvc   # Generate VS2022 solution with v90 toolset
build_vs.bat debug vs2019 clang    # Generate VS2019 solution with VS2008 Clang toolchain
build_vs.bat release vs2022 sdk70a # Generate VS2022 solution with SDK 7.0A Clang toolchain
build_vs.bat debug vs2019 sdk build # Generate VS2019 solution with SDK 7.0 Clang toolchain and build
```

### Manual build

#### For Clang-based builds:

1. Create a build directory:
   ```
   mkdir build
   cd build
   ```

2. Configure CMake with the appropriate toolchain:

   For VS2008 toolchain with Clang:
   ```
   cmake -G "NMake Makefiles" -DCMAKE_TOOLCHAIN_FILE=../vs2008-clang-toolchain.cmake -DCMAKE_BUILD_TYPE=Debug ..
   ```

   For SDK 7.0 toolchain with Clang:
   ```
   cmake -G "NMake Makefiles" -DCMAKE_TOOLCHAIN_FILE=../sdk70-clang-toolchain.cmake -DSDK_VERSION=7.0 -DCMAKE_BUILD_TYPE=Debug ..
   ```

   For SDK 7.0A toolchain with Clang:
   ```
   cmake -G "NMake Makefiles" -DCMAKE_TOOLCHAIN_FILE=../sdk70-clang-toolchain.cmake -DSDK_VERSION=7.0A -DCMAKE_BUILD_TYPE=Debug ..
   ```

   You can also customize SDK paths if needed:
   ```
   cmake -G "NMake Makefiles" -DCMAKE_TOOLCHAIN_FILE=../sdk70-clang-toolchain.cmake -DSDK_VERSION=7.0 -DSDK_PATH="C:/Path/To/SDK" -DVS90_PATH="C:/Path/To/VS9" -DCMAKE_BUILD_TYPE=Debug ..
   ```

3. Build the project:
   ```
   cmake --build . --config Debug
   ```

#### For Visual Studio with any toolchain:

1. Create a build directory:
   ```
   mkdir build_vs
   cd build_vs
   ```

2. Configure CMake for Visual Studio with your preferred toolchain:

   For Visual Studio 2019 with v90 toolset:
   ```
   cmake -G "Visual Studio 16 2019" -A Win32 -T v90 -DUSE_CLANG=OFF -DUSE_VS_IDE=ON ..
   ```

   For Visual Studio 2022 with v90 toolset:
   ```
   cmake -G "Visual Studio 17 2022" -A Win32 -T v90 -DUSE_CLANG=OFF -DUSE_VS_IDE=ON ..
   ```

   For Visual Studio 2019 with VS2008 Clang toolchain:
   ```
   cmake -G "Visual Studio 16 2019" -A Win32 -DCMAKE_TOOLCHAIN_FILE=../vs2008-clang-toolchain.cmake -DUSE_CLANG=ON -DUSE_VS_IDE=ON ..
   ```

   For Visual Studio 2019 with SDK 7.0 Clang toolchain:
   ```
   cmake -G "Visual Studio 16 2019" -A Win32 -DCMAKE_TOOLCHAIN_FILE=../sdk70-clang-toolchain.cmake -DSDK_VERSION=7.0 -DUSE_CLANG=ON -DUSE_VS_IDE=ON ..
   ```

3. Build the project:
   ```
   cmake --build . --config Debug
   ```

   Or open the generated solution file in Visual Studio and build from there.

## Build System Details

### Toolchain Options

The build system provides three toolchain options:

#### VS2008 Toolchain with Clang (`vs2008-clang-toolchain.cmake`)

This toolchain uses the Visual Studio 2008 environment and configures CMake to use Clang as the compiler. It requires the VS90COMNTOOLS environment variable to be set.

#### SDK 7.0/7.0A Toolchain with Clang (`sdk70-clang-toolchain.cmake`)

This toolchain uses the Windows SDK 7.0 or 7.0A and Visual Studio 9.0 includes without requiring the full Visual Studio 2008 installation. It's particularly useful for CI environments where you might install just the SDK and necessary includes.

Both Clang-based toolchains provide:
- Modern C++ features and better compiler diagnostics
- Improved optimization capabilities
- Binary compatibility with the original game executable

#### Visual Studio 2019/2022 with v90 Toolset

This option uses modern Visual Studio (2019 or 2022) with the v90 platform toolset. It provides:
- Modern IDE features and debugging experience
- Compatibility with the original game binary through the v90 toolset
- No dependency on clang.cpp
- Familiar Visual Studio development workflow

#### Visual Studio 2019/2022 with Clang Toolchains

This option combines the modern Visual Studio IDE with either the VS2008 Clang toolchain or the SDK 7.0/7.0A Clang toolchain. It provides:
- Modern IDE features and debugging experience
- The benefits of Clang's compiler features and optimizations
- Compatibility with the original game binary
- Uses clang.cpp for additional compatibility

### Build Process

The build process follows these steps:

1. Updates the commit ID using the existing batch file
2. Builds clang.cpp
3. Builds the precompiled header
4. Compiles all source files
5. Links everything into the final DLL

### Output

The build output follows standard CMake conventions:
- `build/bin/Debug` or `build_vs_*/bin/Debug` for Debug builds
- `build/bin/Release` or `build_vs_*/bin/Release` for Release builds

You can also automatically copy the DLL to your game directory by setting the `GAME_DIR` environment variable before running the build scripts:
```
set GAME_DIR=C:\Path\To\Civ5\Folder
build.bat debug
```

## Compatibility

The built DLL maintains ABI compatibility with the main game executable by:
- Linking against the same C/C++ runtime as the main game
- Using the same struct layouts, calling conventions, and memory models
- Using compatible compiler and linker flags

### VS2008 Toolchain with Clang Compatibility
- Sets up the VS2008 environment for each compilation step
- Uses VS2008 libraries and includes
- Uses clang.cpp for additional compatibility

### SDK Toolchain with Clang Compatibility
- Uses Windows SDK 7.0/7.0A includes and libraries
- Uses Visual Studio 9.0 includes
- Links against msvcrt.lib for runtime compatibility
- Uses clang.cpp for additional compatibility

### Visual Studio 2019/2022 with v90 Toolset Compatibility
- Uses the v90 platform toolset from Visual Studio
- Maintains binary compatibility through the toolset
- Does not require clang.cpp
- Uses the same compiler and linker flags as the original project

### Visual Studio 2019/2022 with Clang Toolchains Compatibility
- Combines modern Visual Studio IDE with Clang compiler
- Uses either VS2008 environment or SDK 7.0/7.0A
- Includes clang.cpp in the build
- Maintains binary compatibility with the original game

## Customization

To modify the build system:

- Edit `CMakeLists.txt` to change build settings, add/remove source files, etc.
- Edit `vs2008-clang-toolchain.cmake` to change VS2008 toolchain settings
- Edit `sdk70-clang-toolchain.cmake` to change SDK toolchain settings
- Edit `build.bat` or `build_vs.bat` to change the build process

### SDK Path Customization

If your SDK or VS9.0 is installed in a non-standard location, you can customize the paths:

```
cmake -G "NMake Makefiles" -DCMAKE_TOOLCHAIN_FILE=../sdk70-clang-toolchain.cmake -DSDK_PATH="C:/Custom/SDK/Path" -DVS90_PATH="C:/Custom/VS9/Path" -DCMAKE_BUILD_TYPE=Debug ..
```

This is particularly useful for CI environments where you might install the SDK and VS9.0 includes in custom locations.

### Visual Studio Toolset Customization

If you need to use a specific version of the v90 toolset or have it installed in a non-standard location, you can customize the toolset specification:

```
cmake -G "Visual Studio 16 2019" -A Win32 -T v90,host=x64 -DUSE_CLANG=OFF -DUSE_VS_IDE=ON ..
```

You can also specify a custom toolset location if needed.