option(AVT_ENABLE_CACHE "Enable cache if available" ON)

if(NOT AVT_ENABLE_CACHE)
  return()
endif()

#
# CCACHE how to use - https://docs.avt.striketechnologies.com/display/INFRASTRUCTURE/CCACHE
#

function(check_ccache_conf)
  set(CCACHE_CONF_CONTENT "")
  execute_process(COMMAND ${CACHE_BINARY} -p
                  OUTPUT_VARIABLE CCACHE_CONF_RAW_OUTPUT)
  string(REGEX MATCH "^.*remote_storage.*=.*redis.*" CCACHE_CONF_CONTENT ${CCACHE_CONF_RAW_OUTPUT})
  if ("${CCACHE_CONF_CONTENT}" STREQUAL "")
    message(STATUS "For remote cache, please use ${CMAKE_SOURCE_DIR}/cmake/ccache.conf as \$HOME/.config/ccache/ccache.conf")
  endif()
endfunction(check_ccache_conf)

set(CACHE_OPTION
  "ccache"
  CACHE STRING "Compiler cache to be used")
set(CACHE_OPTION_VALUES "ccache" "sccache")
set_property(CACHE CACHE_OPTION PROPERTY STRINGS ${CACHE_OPTION_VALUES})
list(
  FIND
  CACHE_OPTION_VALUES
  ${CACHE_OPTION}
  CACHE_OPTION_INDEX)

if(${CACHE_OPTION_INDEX} EQUAL -1)
  message(
    STATUS
    "Using custom compiler cache system: '${CACHE_OPTION}', explicitly supported entries are ${CACHE_OPTION_VALUES}")
endif()

find_program(CACHE_BINARY ${CACHE_OPTION} PATH_SUFFIXES "/srv/opt/ccache/ccache-4.7.4/bin/")

if(CACHE_BINARY)
  # Runs ccache command to get the version
  execute_process(COMMAND ${CACHE_BINARY} --version
                  COMMAND head -1
                  OUTPUT_VARIABLE CCACHE_VERSION_RAW_OUTPUT)

  # Extracts the version from the output of the command run before
  string(STRIP ${CCACHE_VERSION_RAW_OUTPUT} CCACHE_VERSION_RAW_OUTPUT)
  string(REGEX MATCH "ccache.*version 4\.[4-9].*" CCACHE_VERSION "${CCACHE_VERSION_RAW_OUTPUT}")
  if ("${CCACHE_VERSION}" STREQUAL "")
    message(WARNING "${CACHE_OPTION} is enabled but invalid version detected (${CCACHE_VERSION_RAW_OUTPUT}). Not using it")
  else()
    message(STATUS "${CACHE_OPTION} found and enabled (${CCACHE_VERSION})")
    set(CMAKE_C_COMPILER_LAUNCHER ${CACHE_BINARY})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CACHE_BINARY})
    check_ccache_conf()
  endif()
else()
  message(WARNING "${CACHE_OPTION} is enabled but was not found. Not using it")
endif()
