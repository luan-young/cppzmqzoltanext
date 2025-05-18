cmake_minimum_required(VERSION 3.27)

# Ensure non-empty default build type for single-config
get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT isMultiConfig)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif()

project(CppZmqZoltanExt VERSION 0.0.1 LANGUAGES CXX)

include(FetchContent)

# Require C++11, but let a parent project ask for something higher
if(DEFINED CMAKE_CXX_STANDARD)
	if(CMAKE_CXX_STANDARD EQUAL 98 OR CMAKE_CXX_STANDARD LESS 11)
		message(FATAL_ERROR "${PROJECT_NAME} requires at least C++11")
	endif()
else()
	set(CMAKE_CXX_STANDARD 11)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable compiler extensions, but let a parent ask for them
if(NOT DEFINED CMAKE_CXX_EXTENSIONS)
	set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

option(CZZE_BUILD_TESTS "Build tests" OFF)

find_package(cppzmq REQUIRED)

add_subdirectory(src)

if(CZZE_BUILD_TESTS)
    enable_testing()
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest
        GIT_TAG b514bdc898e2951020cbdca1304b75f5950d1f59
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    add_subdirectory(tests)
endif()
