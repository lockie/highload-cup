cmake_minimum_required(VERSION 3.8)

project(server LANGUAGES C)

if(CMAKE_BUILD_TYPE STREQUAL "")
  # CMake defaults to leaving CMAKE_BUILD_TYPE empty. This screws up
  # differentiation between debug and release builds.
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build, options are: None (CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif ()

set(CMAKE_DEBUG_POSTFIX "_d")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -ffunction-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")

set(SRCS
  ./main.c
)

# add assembler listing
foreach(FILE ${SRCS})
  get_filename_component(NAME ${FILE} NAME_WE)
  set_source_files_properties (${FILE}
    PROPERTIES
    COMPILE_FLAGS "-fverbose-asm -masm=intel -Wa,-adhln=${NAME}.s")
endforeach(FILE in ${SRCS})

include_directories(
  ${LIBEVENT_INCLUDE_DIRS}
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(server ${SRCS})
target_link_libraries(server event_static)
