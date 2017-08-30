project(server LANGUAGES C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

include(${CMAKE_SOURCE_DIR}/cmake/Gengetopt.cmake)

find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

find_package(JeMalloc REQUIRED)

find_package(Gperftools)


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -Wall -Wextra -pedantic")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")

set(SRCS
  main.c
  database.c
  request.c
  response.c
  entity.c
  methods.c
)

add_gengetopt_files(SRCS cmdline.ggo)

# add assembler listing. XXX not working with LTO
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  if(CMAKE_COMPILER_IS_GNUCC)
    foreach(FILE ${SRCS})
      get_filename_component(NAME ${FILE} NAME_WE)
      set_source_files_properties (${FILE}
        PROPERTIES
        COMPILE_FLAGS "-fverbose-asm -Wa,-adhln=${NAME}.s")
    endforeach(FILE in ${SRCS})
  endif()
endif()


if(GPERFTOOLS_FOUND)
  include_directories(${GPERFTOOLS_INCLUDE_DIR})
endif()

include_directories(
  ${JEMALLOC_INCLUDE_DIR}
  ${GLIB_INCLUDE_DIRS}
  ${LIBEVENT_INCLUDE_DIRS}
  "${CMAKE_SOURCE_DIR}/vendor/cJSON"
  "${CMAKE_SOURCE_DIR}/vendor/miniz"
  "${CMAKE_CURRENT_BINARY_DIR}"
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(server ${SRCS})
target_link_libraries(server
  event_static event_pthreads_static cjson miniz dl
  ${CMAKE_THREAD_LIBS_INIT}
  ${GLIB_LIBRARIES}
  ${JEMALLOC_LIBRARIES}
)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  if(GPERFTOOLS_FOUND)
    target_link_libraries(server ${GPERFTOOLS_LIBRARIES})
  endif()
endif()
