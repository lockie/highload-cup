# Copyright (c) 2009 Christian Gierds
# License AGPLv3+: GNU Affero General Public License version 3 or later
#  <http://www.gnu.org/licenses/agpl.html>
# Source: https://git.io/v5sGj

# flex a .ll file

# search flex
MACRO(FIND_GENGETOPT)
    IF(NOT GENGETOPT_EXECUTABLE)
        FIND_PROGRAM(GENGETOPT_EXECUTABLE gengetopt)
        IF (NOT GENGETOPT_EXECUTABLE)
          MESSAGE(FATAL_ERROR "gengetopt not found - aborting")
        ENDIF (NOT GENGETOPT_EXECUTABLE)
    ENDIF(NOT GENGETOPT_EXECUTABLE)
ENDMACRO(FIND_GENGETOPT)

MACRO(ADD_GENGETOPT_FILES _sources )
    FIND_GENGETOPT()

    FOREACH (_current_FILE ${ARGN})
        GET_FILENAME_COMPONENT(_in ${_current_FILE} ABSOLUTE)
        GET_FILENAME_COMPONENT(_basename ${_current_FILE} NAME_WE)

      SET(_out ${CMAKE_CURRENT_BINARY_DIR}/${_basename}.c)
      SET(_header ${CMAKE_CURRENT_BINARY_DIR}/${_basename}.h)

    ADD_CUSTOM_COMMAND(
       OUTPUT ${_out} ${_header}
       COMMAND ${GENGETOPT_EXECUTABLE}
       ARGS
       --input=${_in}
       DEPENDS ${_in}
    )

    SET(${_sources} ${${_sources}} ${_out} )
    SET(${_sources} ${${_sources}} ${_header} )
    ENDFOREACH (_current_FILE)
ENDMACRO(ADD_GENGETOPT_FILES)
