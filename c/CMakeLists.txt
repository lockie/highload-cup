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
  "${CMAKE_C_FLAGS_RELEASE} -fomit-frame-pointer -ffunction-sections -fdata-sections -flto -fno-fat-lto-objects")
set(CMAKE_C_FLAGS_RELWITHDEBINFO
  "${CMAKE_C_FLAGS_RELWITHDEBINFO} -fomit-frame-pointer -ffunction-sections -fdata-sections")
set(CMAKE_C_FLAGS_MINSIZEREL
  "${CMAKE_C_FLAGS_MINSIZEREL} -fomit-frame-pointer -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")

if(CMAKE_COMPILER_IS_GNUCC)
  SET(CMAKE_AR  "gcc-ar")
  SET(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
  SET(CMAKE_C_ARCHIVE_FINISH   true)
endif()

# Libevent #####################################################################
set(EVENT__DISABLE_THREAD_SUPPORT OFF CACHE BOOL "")
set(EVENT__DISABLE_OPENSSL ON CACHE BOOL "")
set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "")
set(EVENT__DISABLE_TESTS ON CACHE BOOL "")
set(EVENT__DISABLE_REGRESS ON CACHE BOOL "")
set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "")

add_subdirectory("vendor/libevent")
# use static, no need in shared
set_target_properties(
  event_shared event_core_shared event_extra_shared event_pthreads_shared
  PROPERTIES EXCLUDE_FROM_ALL ON EXCLUDE_FROM_DEFAULT_BUILD ON)
# use generic event_shared, no need in those separate ones
set_target_properties(
  event_core_static event_extra_static event_pthreads_static
  PROPERTIES EXCLUDE_FROM_ALL ON EXCLUDE_FROM_DEFAULT_BUILD ON)

# cJSON ########################################################################

set(ENABLE_CJSON_TEST OFF CACHE BOOL "")
set(ENABLE_CUSTOM_COMPILER_FLAGS OFF CACHE BOOL "")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")

add_subdirectory("vendor/cJSON")
set_target_properties(cjson
  PROPERTIES COMPILE_DEFINITIONS CJSON_NESTING_LIMIT=4)

# SQLite #######################################################################

add_subdirectory("vendor/sqlite")

# Miniz ########################################################################

add_subdirectory("vendor/miniz")
set_target_properties(example1 example2 example3 example4 example5 example6
  PROPERTIES EXCLUDE_FROM_ALL ON EXCLUDE_FROM_DEFAULT_BUILD ON)
target_compile_definitions(miniz PRIVATE
  _LARGEFILE64_SOURCE
  MINIZ_DISABLE_ZIP_READER_CRC32_CHECKS=1
  MINIZ_NO_ARCHIVE_WRITING_APIS=1
  MINIZ_NO_ZLIB_APIS=1)

# Main binary ##################################################################
add_subdirectory("src")
