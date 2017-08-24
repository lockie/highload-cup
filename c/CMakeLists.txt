cmake_minimum_required(VERSION 3.8)

project(HighloadCup LANGUAGES C)

# General config ###############################################################

if(CMAKE_BUILD_TYPE STREQUAL "")
  # CMake defaults to leaving CMAKE_BUILD_TYPE empty. This screws up
  # differentiation between debug and release builds.
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
    "Choose the type of build, options are: None (CMAKE_CXX_FLAGS or \
CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif ()

set(CMAKE_DEBUG_POSTFIX "_d")

add_compile_options(-march=sandybridge -pipe)

set(CMAKE_C_FLAGS_RELEASE
  "${CMAKE_C_FLAGS_RELEASE} -fomit-frame-pointer")
set(CMAKE_C_FLAGS_RELWITHDEBINFO
  "${CMAKE_C_FLAGS_RELWITHDEBINFO} -fomit-frame-pointer")
set(CMAKE_C_FLAGS_MINSIZEREL
  "${CMAKE_C_FLAGS_MINSIZEREL} -fomit-frame-pointer")


# Libevent #####################################################################
set(EVENT__DISABLE_THREAD_SUPPORT OFF CACHE BOOL "")
set(EVENT__DISABLE_OPENSSL ON CACHE BOOL "")
set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "")
set(EVENT__DISABLE_TESTS ON CACHE BOOL "")
set(EVENT__DISABLE_REGRESS ON CACHE BOOL "")
set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "")
set(EVENT__ENABLE_GCC_FUNCTION_SECTIONS ON CACHE BOOL "")

add_subdirectory("vendor/libevent")
# use static, no need in shared
set_target_properties(
  event_shared event_core_shared event_extra_shared event_pthreads_shared
  PROPERTIES EXCLUDE_FROM_ALL ON)
# use generic event_shared, no need in those separate ones
set_target_properties(
  event_core_static event_extra_static event_pthreads_static
  PROPERTIES EXCLUDE_FROM_ALL ON)

# cJSON ########################################################################

set(ENABLE_CJSON_TEST OFF CACHE BOOL "")
set(ENABLE_CUSTOM_COMPILER_FLAGS OFF CACHE BOOL "")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
set(custom_compiler_flags "-ffunction-sections" CACHE STRING "")

add_subdirectory("vendor/cJSON")
set_target_properties(cjson
  PROPERTIES COMPILE_DEFINITIONS "CJSON_NESTING_LIMIT=4")

# SQLite #######################################################################

add_subdirectory("vendor/sqlite")

# Main binary ##################################################################
add_subdirectory("src")
