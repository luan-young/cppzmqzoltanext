cmake_minimum_required(VERSION 3.27)

# ---------------------------------------------------------------------------------------
# Project definition
# ---------------------------------------------------------------------------------------

project(cppzmqzoltanext VERSION 0.0.1 LANGUAGES CXX)

# ---------------------------------------------------------------------------------------
# Compiler settings
# ---------------------------------------------------------------------------------------

# Require C++17, but let a parent project ask for something higher
if(DEFINED CMAKE_CXX_STANDARD)
	if(CMAKE_CXX_STANDARD EQUAL 98 OR CMAKE_CXX_STANDARD LESS 17)
		message(FATAL_ERROR "${PROJECT_NAME} requires at least C++17")
	endif()
else()
	set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable compiler extensions, but let a parent ask for them
if(NOT DEFINED CMAKE_CXX_EXTENSIONS)
	set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

# ---------------------------------------------------------------------------------------
# Set default build type to Release
# ---------------------------------------------------------------------------------------

# Ensure non-empty default build type for single-config
get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT isMultiConfig)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif()

# ---------------------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------------------

option(CZZE_BUILD_TESTS "Build tests" OFF)
option(CZZE_ENABLE_ASAN "Enable AddressSanitizer" OFF)

# ---------------------------------------------------------------------------------------
# CMake modules
# ---------------------------------------------------------------------------------------

include(FetchContent)
include(GenerateExportHeader)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ---------------------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------------------

find_package(cppzmq REQUIRED)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------------------
# Sanitizers
# ---------------------------------------------------------------------------------------

if(CZZE_ENABLE_ASAN)
    message(STATUS "Enabling AddressSanitizer")

    set(ASAN_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}")

endif()

# ---------------------------------------------------------------------------------------
# Targets subdirectories
# ---------------------------------------------------------------------------------------

add_subdirectory(src)

# ---------------------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------------------

if(CZZE_BUILD_TESTS)
    enable_testing()
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest
        GIT_TAG b514bdc898e2951020cbdca1304b75f5950d1f59
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF)
    FetchContent_MakeAvailable(googletest)
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------------------

if(APPLE)
    set(base @loader_path)
else()
    set(base $ORIGIN)
endif()
file(RELATIVE_PATH relDir
	${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}
	${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}
)
set(CMAKE_INSTALL_RPATH ${base} ${base}/${relDir})

install(TARGETS libcppzmqzoltanext libcppzmqzoltanextstatic
    EXPORT ${PROJECT_NAME}_ExportTargets
    RUNTIME
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
	FILE_SET HEADERS
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT ${PROJECT_NAME}_ExportTargets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE ${PROJECT_NAME}::
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

write_basic_package_version_file(
    ${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
