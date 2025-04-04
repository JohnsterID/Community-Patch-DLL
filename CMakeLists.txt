cmake_minimum_required(VERSION 3.15)
project(CommunityPatchDLL LANGUAGES CXX)

# Set the name of the core DLL
set(CORE_DLL "CvGameCore_Expansion2")

# Options
option(USE_CLANG "Use Clang compiler instead of MSVC" ON)
option(USE_VS_IDE "Generate Visual Studio solution with v90 toolset" OFF)

# Determine build environment type
if(CMAKE_TOOLCHAIN_FILE MATCHES "sdk70-clang-toolchain.cmake")
    set(USE_SDK TRUE)
    message(STATUS "Using Windows SDK 7.0/7.0A build environment")
else()
    set(USE_SDK FALSE)
    # Check for VS2008 environment
    if(NOT DEFINED ENV{VS90COMNTOOLS})
        message(FATAL_ERROR "VS90COMNTOOLS environment variable not found. Visual Studio 2008 must be installed.")
    endif()
    
    # Set VS2008 environment path
    set(VS_2008_VARS_BAT "$ENV{VS90COMNTOOLS}/vsvars32.bat")
    message(STATUS "Using VS2008 build environment: ${VS_2008_VARS_BAT}")
endif()

# Configure compiler
if(USE_CLANG)
    set(CMAKE_CXX_COMPILER "clang-cl.exe")
    set(CMAKE_LINKER "lld-link.exe")
endif()

# Set build directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Set configuration-specific output directories
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_BINARY_DIR}/bin/${OUTPUTCONFIG}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_BINARY_DIR}/lib/${OUTPUTCONFIG}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_BINARY_DIR}/lib/${OUTPUTCONFIG}")
    set(CMAKE_PDB_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_BINARY_DIR}/bin/${OUTPUTCONFIG}")
endforeach(OUTPUTCONFIG)

# Set build type specific compiler flags
if(USE_VS_IDE)
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /MDd")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /MD /Zi")
    
    # Set common compiler flags for VS IDE
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GS /EHsc /fp:precise /Zc:wchar_t /W3 /Zm300")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "/Od -g")
    set(CMAKE_CXX_FLAGS_RELEASE "/Ox /Ob2 -flto")
    
    # Set common compiler flags for Clang
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -msse3 /c /MD /GS /EHsc /fp:precise /Zc:wchar_t /Z7 /W1")
endif()

# Suppress specific warnings
if(NOT USE_VS_IDE)
    set(CL_SUPPRESS 
        "invalid-offsetof"
        "tautological-constant-out-of-range-compare"
        "comment"
        "enum-constexpr-conversion"
        "c++11-narrowing"
    )

    foreach(SUPPRESS ${CL_SUPPRESS})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-${SUPPRESS}")
    endforeach()
endif()

# Set linker flags
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MACHINE:x86 /DLL /DEBUG /LTCG /DYNAMICBASE /NXCOMPAT /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /FORCE:MULTIPLE")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF")

# Define include directories
set(INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2"
    "${CMAKE_SOURCE_DIR}/CvWorldBuilderMap/include"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLLUtil/include"
    "${CMAKE_SOURCE_DIR}/CvLocalization/include"
    "${CMAKE_SOURCE_DIR}/CvGameDatabase/include"
    "${CMAKE_SOURCE_DIR}/FirePlace/include"
    "${CMAKE_SOURCE_DIR}/FirePlace/include/FireWorks"
    "${CMAKE_SOURCE_DIR}/ThirdPartyLibs/Lua51/include"
)

# Define preprocessor definitions
set(SHARED_PREDEFS
    "FXS_IS_DLL"
    "WIN32"
    "_WINDOWS"
    "_USRDLL"
    "EXTERNAL_PAUSING"
    "CVGAMECOREDLL_EXPORTS"
    "FINAL_RELEASE"
    "_CRT_SECURE_NO_WARNINGS"
    "_WINDLL"
)

set(RELEASE_PREDEFS
    ${SHARED_PREDEFS}
    "STRONG_ASSUMPTIONS"
    "NDEBUG"
    "VPRELEASE_ERRORMSG"
)

set(DEBUG_PREDEFS
    ${SHARED_PREDEFS}
    "VPDEBUG"
)

# Define libraries
set(LIBS
    "${CMAKE_SOURCE_DIR}/CvWorldBuilderMap/lib/CvWorldBuilderMapWin32.obj"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLLUtil/lib/CvGameCoreDLLUtilWin32.lib"
    "${CMAKE_SOURCE_DIR}/CvLocalization/lib/CvLocalizationWin32.lib"
    "${CMAKE_SOURCE_DIR}/CvGameDatabase/lib/CvGameDatabaseWin32.lib"
    "${CMAKE_SOURCE_DIR}/FirePlace/lib/FireWorksWin32.obj"
    "${CMAKE_SOURCE_DIR}/FirePlace/lib/FLuaWin32.lib"
    "${CMAKE_SOURCE_DIR}/ThirdPartyLibs/Lua51/lib/lua51_Win32.lib"
)

set(DEFAULT_LIBS
    "winmm.lib"
    "kernel32.lib"
    "user32.lib"
    "gdi32.lib"
    "winspool.lib"
    "comdlg32.lib"
    "advapi32.lib"
    "shell32.lib"
    "ole32.lib"
    "oleaut32.lib"
    "uuid.lib"
    "odbc32.lib"
    "odbccp32.lib"
)

# Define source files
set(PCH_CPP "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/_precompile.cpp")
set(PCH_H "CvGameCoreDLLPCH.h")
set(CLANG_CPP "${CMAKE_SOURCE_DIR}/clang.cpp")
set(DEF_FILE "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvGameCoreDLL.def")

# Add all the source files
set(SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaArea.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaArgsHandle.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaCity.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaDeal.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaEnums.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaFractal.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaGame.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaGameInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaLeague.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaMap.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaPlayer.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaPlot.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaSupport.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaTeam.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaTeamTech.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/Lua/CvLuaUnit.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CustomMods.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvAchievementInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvAchievementUnlocker.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvAdvisorCounsel.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvAdvisorRecommender.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvAIOperation.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvArea.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvArmyAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvAStar.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvAStarNode.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvBarbarians.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvBeliefClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvBuilderTaskingAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvBuildingClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvBuildingProductionAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvCity.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvCityAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvCityCitizens.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvCityConnections.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvCityManager.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvCitySpecializationAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvCityStrategyAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvContractClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvCorporationClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvCultureClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDangerPlots.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDatabaseUtility.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDealAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDealClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDiplomacyAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDiplomacyRequests.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDistanceMap.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllBuildInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllBuildingInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllCity.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllCivilizationInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllColorInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllCombatInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllContext.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllDatabaseUtility.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllDeal.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllDealAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllDiplomacyAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllDlcPackageInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllEraInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllFeatureInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllGame.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllGameAsynch.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllGameDeals.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllGameOptionInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllGameSpeedInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllHandicapInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllImprovementInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllInterfaceModeInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllLeaderheadInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllMap.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllMinorCivInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllMissionData.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllMissionInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllNetInitInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllNetLoadGameInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllNetMessageExt.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllNetMessageHandler.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllNetworkSyncronization.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllPathFinderUpdate.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllPlayer.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllPlayerColorInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllPlayerOptionInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllPlot.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllPolicyInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllPreGame.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllPromotionInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllRandom.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllResourceInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllScriptSystemUtility.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllTeam.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllTechInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllTerrainInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllUnit.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllUnitCombatClassInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllUnitInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllVictoryInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllWorldBuilderMapLoader.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvDllWorldInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvEconomicAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvEmphasisClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvEspionageClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvEventLog.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvFlavorManager.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvFractal.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvGame.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvGameCoreDLL.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvGameCoreEnumSerialization.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvGameCoreStructs.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvGameCoreUtils.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvGameQueries.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvGameTextMgr.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvGlobals.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvGoodyHuts.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvGrandStrategyAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvGreatPersonInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvHomelandAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvImprovementClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvInfos.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvInfosSerializationHelper.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvInternalGameCoreUtils.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvLoggerCSV.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvMap.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvMapGenerator.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvMilitaryAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvMinorCivAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvNotificationClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvNotifications.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvPlayer.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvPlayerAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvPlayerManager.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvPlot.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvPlotInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvPlotManager.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvPolicyAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvPolicyClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvPopupInfoSerialization.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvPreGame.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvProcessProductionAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvProjectClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvProjectProductionAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvPromotionClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvRandom.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvReligionClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvReplayInfo.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvReplayMessage.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvSerialize.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvSiteEvaluationClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvStartPositioner.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/cvStopWatch.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvTacticalAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvTacticalAnalysisMap.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvTargeting.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvTeam.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvTechAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvTechClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvTradeClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvTraitClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvTreasury.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvTypes.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvUnit.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvUnitClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvUnitCombat.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvUnitCycler.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvUnitMission.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvUnitMovement.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvUnitProductionAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvVotingClasses.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvWonderProductionAI.cpp"
    "${CMAKE_SOURCE_DIR}/CvGameCoreDLL_Expansion2/CvWorldBuilderMapLoader.cpp"
)

# Create a custom command to update commit ID
add_custom_target(update_commit_id
    COMMAND ${CMAKE_SOURCE_DIR}/update_commit_id.bat
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Updating commit ID"
    VERBATIM
)

# Create a custom command to build clang.cpp (only for Clang builds)
if(USE_CLANG)
    if(USE_SDK)
        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/clang.obj
            COMMAND ${CMAKE_COMMAND} -E echo "Building clang.cpp..."
            COMMAND ${CMAKE_CXX_COMPILER} "${CLANG_CPP}" /Fo"${CMAKE_BINARY_DIR}/clang.obj" ${CMAKE_CXX_FLAGS}
            DEPENDS ${CLANG_CPP}
            COMMENT "Building clang.cpp"
            VERBATIM
        )
    else()
        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/clang.obj
            COMMAND ${CMAKE_COMMAND} -E echo "Building clang.cpp..."
            COMMAND cmd /c "\"${VS_2008_VARS_BAT}\">NUL && ${CMAKE_CXX_COMPILER} \"${CLANG_CPP}\" /Fo\"${CMAKE_BINARY_DIR}/clang.obj\" ${CMAKE_CXX_FLAGS}"
            DEPENDS ${CLANG_CPP}
            COMMENT "Building clang.cpp"
            VERBATIM
        )
    endif()

    # Create a custom target for clang.cpp
    add_custom_target(clang_cpp DEPENDS ${CMAKE_BINARY_DIR}/clang.obj)
endif()

# Create the DLL target
add_library(${CORE_DLL} SHARED ${SOURCE_FILES})

# Add include directories
target_include_directories(${CORE_DLL} PRIVATE ${INCLUDE_DIRS})

# Add preprocessor definitions based on build type
target_compile_definitions(${CORE_DLL} PRIVATE 
    $<$<CONFIG:Debug>:${DEBUG_PREDEFS}>
    $<$<CONFIG:Release>:${RELEASE_PREDEFS}>
)

# Set up precompiled header
if(USE_SDK)
    target_compile_options(${CORE_DLL} PRIVATE /Yu"${PCH_H}" /Fp"${CMAKE_BINARY_DIR}/${PCH}")
    set_source_files_properties(${PCH_CPP} PROPERTIES COMPILE_FLAGS "/Yc\"${PCH_H}\" /Fp\"${CMAKE_BINARY_DIR}/${PCH}\"")
else()
    target_compile_options(${CORE_DLL} PRIVATE /Yu"${PCH_H}" /Fp"${CMAKE_BINARY_DIR}/${PCH}")
    set_source_files_properties(${PCH_CPP} PROPERTIES COMPILE_FLAGS "/Yc\"${PCH_H}\" /Fp\"${CMAKE_BINARY_DIR}/${PCH}\"")
endif()

# Set module definition file
set_target_properties(${CORE_DLL} PROPERTIES 
    LINK_FLAGS "/DEF:\"${DEF_FILE}\""
)

# Add libraries
target_link_libraries(${CORE_DLL} PRIVATE ${LIBS} ${DEFAULT_LIBS})

# Add msvcrt.lib when using SDK
if(USE_SDK)
    target_link_libraries(${CORE_DLL} PRIVATE msvcrt.lib)
endif()

# Add dependencies
if(USE_CLANG AND NOT USE_VS_IDE)
    add_dependencies(${CORE_DLL} update_commit_id clang_cpp)
    
    # Add clang.obj to the link
    target_link_libraries(${CORE_DLL} PRIVATE "${CMAKE_BINARY_DIR}/clang.obj")
else()
    add_dependencies(${CORE_DLL} update_commit_id)
endif()

# Create a custom command to run the build with VS2008 environment (if not using SDK)
if(NOT USE_SDK)
    add_custom_command(
        TARGET ${CORE_DLL} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Setting up VS2008 environment..."
        COMMAND cmd /c "\"${VS_2008_VARS_BAT}\">NUL"
        COMMENT "Setting up VS2008 environment"
        VERBATIM
    )
endif()

# Create a custom target for building with VS2008 environment
add_custom_target(build_with_vs2008
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG>
    DEPENDS update_commit_id
    COMMENT "Building with VS2008 environment"
    VERBATIM
)

# Add a message about the build process
message(STATUS "CMake build system for Community Patch DLL")
message(STATUS "Using Clang with VS2008 environment")
message(STATUS "VS2008 environment: ${VS_2008_VARS_BAT}")