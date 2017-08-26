project(sqlite LANGUAGES C)

set(SQLITE3_URL "https://www.sqlite.org/2017/sqlite-src-3200100.zip")
set(SQLITE3_SHA1 "808448837323b4ef9c0771af89d06dca5f0832dd")

###############################################################################
# NO CHANGE BELOW THIS LINE
###############################################################################

set(SQLITE_WORK_DIR "${CMAKE_BINARY_DIR}/sqlite/src/sqlite3c")

include(ExternalProject)

ExternalProject_Add(sqlite3c
  URL ${SQLITE3_URL}
  URL_HASH SHA1=${SQLITE3_SHA1}
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}
  CONFIGURE_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/sqlite3c/configure -q --disable-tcl --disable-shared --disable-threadsafe --enable-memsys5
  STEP_TARGETS configure amalgamation
  EXCLUDE_FROM_ALL TRUE
  DOWNLOAD_NO_PROGRESS ON
)

ExternalProject_Add_Step(sqlite3c amalgamation
  DEPENDEES configure
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMAND "make" "sqlite3.c"
  BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/sqlite3.c"
)

add_library(SQLite STATIC "${CMAKE_CURRENT_BINARY_DIR}/sqlite3.c")
add_dependencies(SQLite sqlite3c-amalgamation)

target_compile_definitions(SQLite PRIVATE
  _HAVE_SQLITE_CONFIG_H
  SQLITE_THREADSAFE=0
  SQLITE_DEFAULT_MEMSTATUS=0
  SQLITE_MAX_EXPR_DEPTH=0
  SQLITE_OMIT_DECLTYPE
  SQLITE_OMIT_DEPRECATED
  SQLITE_OMIT_PROGRESS_CALLBACK
  SQLITE_OMIT_SHARED_CACHE
  SQLITE_ENABLE_MEMSYS5
  SQLITE_ENABLE_STAT4
  YYSTACKDEPTH=20
)
