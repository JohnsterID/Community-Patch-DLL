# Toolchain file for using Clang with VS2008 environment
# This file configures CMake to use Clang as the compiler while maintaining VS2008 compatibility

# System information
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

# Specify the Clang compiler
set(CMAKE_C_COMPILER clang-cl.exe)
set(CMAKE_CXX_COMPILER clang-cl.exe)
set(CMAKE_LINKER lld-link.exe)
set(CMAKE_RC_COMPILER rc.exe)

# Check for VS2008 environment
if(NOT DEFINED ENV{VS90COMNTOOLS})
    message(FATAL_ERROR "VS90COMNTOOLS environment variable not found. Visual Studio 2008 must be installed.")
endif()

# Set VS2008 environment path
set(VS_2008_VARS_BAT "$ENV{VS90COMNTOOLS}/vsvars32.bat")

# Force 32-bit compilation
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")

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

# Message about toolchain
message(STATUS "Using VS2008-Clang toolchain")
message(STATUS "VS2008 environment: ${VS_2008_VARS_BAT}")