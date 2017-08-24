cmake_minimum_required(VERSION 3.8)

project(HighloadCup LANGUAGES C)

add_compile_options(-march=sandybridge -fomit-frame-pointer -pipe)

# Libevent #####################################################################
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
set_target_properties(cjson PROPERTIES COMPILE_DEFINITIONS "CJSON_NESTING_LIMIT=4")

# Main binary ##################################################################
add_subdirectory("src")
