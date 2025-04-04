# Toolchain file for using Clang with Windows SDK 7.0/7.0A and VS9.0 includes
# This file configures CMake to use Clang as the compiler while maintaining VS2008 compatibility
# without requiring the full Visual Studio 2008 installation

# System information
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

# Specify the Clang compiler
set(CMAKE_C_COMPILER clang-cl.exe)
set(CMAKE_CXX_COMPILER clang-cl.exe)
set(CMAKE_LINKER lld-link.exe)
set(CMAKE_RC_COMPILER rc.exe)

# SDK version - can be overridden via -DSDK_VERSION=7.0A
if(NOT DEFINED SDK_VERSION)
    set(SDK_VERSION "7.0" CACHE STRING "Windows SDK version (7.0 or 7.0A)")
endif()

# SDK and VS9.0 paths - can be overridden via -DSDK_PATH and -DVS90_PATH
if(NOT DEFINED SDK_PATH)
    set(SDK_PATH "C:/Program Files (x86)/Microsoft SDKs/Windows/v${SDK_VERSION}" CACHE PATH "Path to Windows SDK")
endif()

if(NOT DEFINED VS90_PATH)
    set(VS90_PATH "C:/Program Files (x86)/Microsoft Visual Studio 9.0" CACHE PATH "Path to Visual Studio 9.0")
endif()

# Check if paths exist
if(NOT EXISTS "${SDK_PATH}/Include")
    message(WARNING "Windows SDK include path not found: ${SDK_PATH}/Include")
endif()

if(NOT EXISTS "${VS90_PATH}/VC/include")
    message(WARNING "Visual Studio 9.0 include path not found: ${VS90_PATH}/VC/include")
endif()

if(NOT EXISTS "${SDK_PATH}/Lib")
    message(WARNING "Windows SDK library path not found: ${SDK_PATH}/Lib")
endif()

# Set include and library paths
set(SDK_INCLUDE_PATH "${SDK_PATH}/Include")
set(VS90_INCLUDE_PATH "${VS90_PATH}/VC/include")
set(SDK_LIB_PATH "${SDK_PATH}/Lib")

# Force 32-bit compilation
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")

# Add SDK and VS9.0 include paths
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -external:I\"${SDK_INCLUDE_PATH}\" -external:I\"${VS90_INCLUDE_PATH}\"")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -external:I\"${SDK_INCLUDE_PATH}\" -external:I\"${VS90_INCLUDE_PATH}\"")

# Add SDK library path
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LIBPATH:\"${SDK_LIB_PATH}\"")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LIBPATH:\"${SDK_LIB_PATH}\"")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /LIBPATH:\"${SDK_LIB_PATH}\"")

# Set VS2008 compatible runtime
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MD")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MD")

# Set VS2008 compatible exception handling
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")

# Set VS2008 compatible struct alignment and calling conventions
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Zp8")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zp8")

# Disable RTTI to match VS2008 default
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")

# Set VS2008 compatible debug information format
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Z7")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Z7")

# Set VS2008 compatible optimization levels
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Od")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /O2")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")

# Set VS2008 compatible preprocessor definitions
add_definitions(-D_WINDOWS -D_USRDLL -D_CRT_SECURE_NO_WARNINGS)

# Set VS2008 compatible linker flags
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MACHINE:X86")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MACHINE:X86")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /MACHINE:X86")

# Add msvcrt.lib to default libraries
set(CMAKE_C_STANDARD_LIBRARIES "${CMAKE_C_STANDARD_LIBRARIES} msvcrt.lib")
set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} msvcrt.lib")

# Message about toolchain
message(STATUS "Using SDK70-Clang toolchain")
message(STATUS "SDK version: ${SDK_VERSION}")
message(STATUS "SDK include path: ${SDK_INCLUDE_PATH}")
message(STATUS "VS9.0 include path: ${VS90_INCLUDE_PATH}")
message(STATUS "SDK library path: ${SDK_LIB_PATH}")