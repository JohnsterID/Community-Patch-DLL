cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)
project(VoxPopuli_vs2013 CXX)
# Set target arch type if empty. Visual studio solution generator provides it.
if(NOT CMAKE_VS_PLATFORM_NAME)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CMAKE_VS_PLATFORM_NAME "x64")
    else()
        set(CMAKE_VS_PLATFORM_NAME "Win32")
    endif()
endif()
message("${CMAKE_VS_PLATFORM_NAME} architecture in use")

if(NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32"))
    message(FATAL_ERROR "${CMAKE_VS_PLATFORM_NAME} arch is not supported!")
endif()
# Global configuration types
set(CMAKE_CONFIGURATION_TYPES
    "Debug"
    "Release"
    CACHE STRING "" FORCE
)
# Global compiler options
if(MSVC)
    # remove default flags provided with CMake for MSVC
    set(CMAKE_CXX_FLAGS "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
    set(CMAKE_CXX_FLAGS_RELEASE "")
endif()
# Global linker options
if(MSVC)
    # remove default flags provided with CMake for MSVC
    set(CMAKE_EXE_LINKER_FLAGS "")
    set(CMAKE_MODULE_LINKER_FLAGS "")
    set(CMAKE_SHARED_LINKER_FLAGS "")
    set(CMAKE_STATIC_LINKER_FLAGS "")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_DEBUG "${CMAKE_STATIC_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS}")
endif()
####v90 required
if(MSVC)
  # force use of v90 environment variables
  set(CMAKE_C_COMPILER clang-cl)
  set(CMAKE_CXX_COMPILER clang-cl)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fms-compatibility-version=19.00")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LIBPATH:\"$ENV{VS90COMNTOOLS}/../../VC/lib\"")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LIBPATH:\"$ENV{VS90COMNTOOLS}/../../VC/lib\"")
  # set environment variables for v90
  set(VS90COMNTOOLS "$ENV{VS90COMNTOOLS}")
  set(CMAKE_GENERATOR_TOOLSET "v90")
  set(CMAKE_VS_PLATFORM_TOOLSET "v90")
  set(CMAKE_VS_PLATFORM_NAME "Win32")
  # use vsvars32.bat to set up v90 environment
  find_program(vsvars32 "vsvars32.bat" PATHS "${VS90COMNTOOLS}")
  if(NOT vsvars32)
    message(FATAL_ERROR "Failed to find vsvars32.bat")
  endif()
  message("Using vsvars32.bat from ${VS90COMNTOOLS}")
  execute_process(COMMAND "${vsvars32}" "&&" "set" OUTPUT_VARIABLE vs_env)
  string(REGEX REPLACE "\r?\n" ";" vs_env "${vs_env}")
  foreach(env_var ${vs_env})
    string(REGEX MATCH "^[^=]+" env_name ${env_var})
    string(REGEX MATCH "(.*)=.*" env_value ${env_var})
    set(${env_name} "${env_value}")
    message("Setting environment variable ${env_name}=${env_value}")
  endforeach()

  # set v90 toolset for all build types and generators
  set(CMAKE_GENERATOR_TOOLSET "v90" CACHE STRING "Platform Toolset" FORCE)
  set(CMAKE_VS_PLATFORM_TOOLSET "v90" CACHE STRING "Platform Toolset" FORCE)
endif()
# Nuget packages function stub.
function(use_package TARGET PACKAGE VERSION)
    message(WARNING "No implementation of use_package. Create yours. "
                    "Package \"${PACKAGE}\" with version \"${VERSION}\" "
                    "for target \"${TARGET}\" is ignored!")
endfunction()
# Common utils
include(CMake/Utils.cmake)
# Additional Global Settings(add specific info there)
include(CMake/GlobalSettingsInclude.cmake OPTIONAL)
# Use solution folders feature
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# Sub-projects
add_subdirectory(CvGameCoreDLL_Expansion2)