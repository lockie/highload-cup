project(server LANGUAGES C)

include(${CMAKE_SOURCE_DIR}/cmake/Gengetopt.cmake)

find_package(Threads)

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

include_directories(
  ${LIBEVENT_INCLUDE_DIRS}
  "${CMAKE_SOURCE_DIR}/vendor/cJSON"
  "${CMAKE_BINARY_DIR}/vendor/sqlite"
  "${CMAKE_SOURCE_DIR}/vendor/miniz"
  "${CMAKE_CURRENT_BINARY_DIR}"
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(server ${SRCS})
target_link_libraries(server
  event_static event_pthreads_static cjson SQLite miniz dl ${CMAKE_THREAD_LIBS_INIT})
