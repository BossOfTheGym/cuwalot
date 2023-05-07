cmake_minimum_required(VERSION 3.20)

project(cuwalot)

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

# workaround for exported symbols
# TODO : DLLIMPORT/DLLEXPORT macros
# TODO : fix SHARED build
if (MSVC)
	set(BUILD_SHARED_LIBS TRUE)
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()

# cpp standart
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)
add_subdirectory(tests)