cmake_minimum_required(VERSION 3.20)

option(CUWALOT_STATIC_BUILD "enable for static build" YES)
option(CUWALOT_BUILD_TESTS "build test executables(well, they are not proper tests)" YES)

if (CUWALOT_STATIC_BUILD)
	set(CUWALOT_BUILD_TYPE STATIC)
else()
	set(CUWALOT_BUILD_TYPE SHARED)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# setup platform string
if(WIN32)
	set(platform "windows")
elseif(UNIX)
	set(platform "unix")
else()
    message(FATAL_ERROR "unknown platform")
endif()

# __VA_OPT__ fix for MSVC
if (MSVC)
	message(STATUS "MSVC compiler detected")
	add_compile_options("/Zc:preprocessor")
endif()

project(cuwalot VERSION "0.0.0" LANGUAGES CXX)

set(CUWALOT_SOURCE_DIR ${cuwalot_SOURCE_DIR})
set(CUWALOT_BINARY_DIR ${cuwalot_BINARY_DIR})

include(GenerateExportHeader)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(CUWALOT_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})
set(CUWALOT_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})
set(CUWALOT_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
set(CUWALOT_INSTALL_DOCDIR ${CMAKE_INSTALL_DOCDIR})
set(CUWALOT_CONFIG_DIR ${CUWALOT_INSTALL_LIBDIR}/cmake/cuw)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "")

# cpp standart
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)
if (CUWALOT_BUILD_TESTS)
	add_subdirectory(tests)
endif()