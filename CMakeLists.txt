cmake_minimum_required(VERSION 3.0.0)
project(
  EkaNEW
  VERSION 0.1.0
  LANGUAGES C CXX)

# include(CTest) enable_testing()

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

execute_process(COMMAND git rev-parse --short HEAD OUTPUT_VARIABLE EKA_GIT_VER)

set(CMAKE_C_COMPILER ${CMAKE_CXX_COMPILER})
set(CMAKE_CXX_FLAGS
    "
    -O3
    -funroll-loops
    -pipe
    -march=native
    ")

# ##############################################################################
set(EKA_LIB_NAME ekaN)

set(BUILD_DIR ${CMAKE_CURRENT_LIST_DIR}/build)
set(EKA_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/apisrc)

set(libDstDir ${EKA_SRC_DIR}/../${EKA_LIB_NAME}_lib)
file(MAKE_DIRECTORY ${libDstDir})

set(apiDIR ${EKA_SRC_DIR}/compat)

# ##############################################################################
set(SMARTNIC_ROOT ${CMAKE_CURRENT_LIST_DIR}/SmartNIC_SW)
set(SMARTNIC_API_INCL ${SMARTNIC_ROOT}/include)
set(SMARTNIC_DRIVER_INCL ${SMARTNIC_ROOT}/driver)
set(SMARTNIC_LIB_STATIC ${SMARTNIC_ROOT}/lib/libsmartnic.a)

# execute_process(COMMAND ar x ${SMARTNIC_LIB_STATIC}) execute_process(COMMAND
# ar t ${SMARTNIC_LIB_STATIC} OUTPUT_VARIABLE SMARTNIC_LIB_STATIC_FILES)

# ##############################################################################
# libekalwip.so
set(LWIP_DIR ${CMAKE_CURRENT_LIST_DIR}/LWIP)
include(${LWIP_DIR}/src/Filelists.cmake)

set(ekalwip_SRCS ${lwipcore_SRCS} ${lwipcore4_SRCS} ${lwipapi_SRCS}
                 ${LWIP_DIR}/src/netif/ethernet.c)

set(LWIP_LIB_BUILD_INCL ${LWIP_LIB_BUILD_INCL} ${LWIP_DIR}/src/include)
set(LWIP_LIB_BUILD_INCL ${LWIP_LIB_BUILD_INCL} ${LWIP_DIR}/src/include/arch)
set(LWIP_LIB_BUILD_INCL ${LWIP_LIB_BUILD_INCL} ${EKA_SRC_DIR})

set(LWIP_API_INCL ${LWIP_DIR}/src/include)

add_library(ekalwip SHARED ${ekalwip_SRCS})

# set_target_properties(ekalwip PROPERTIES CXX_STANDARD 98)

target_compile_options(ekalwip PRIVATE -std=gnu++98)

target_compile_definitions(ekalwip PRIVATE _GNU_SOURCE)
target_link_libraries(ekalwip rt m Threads::Threads)

target_include_directories(ekalwip PUBLIC ${LWIP_LIB_BUILD_INCL})

set_target_properties(ekalwip PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${libDstDir})
# ----------------------------------------------------------
# libekalwip_static.a

add_library(ekalwip_static STATIC ${ekalwip_SRCS})
set_target_properties(ekalwip_static PROPERTIES CXX_STANDARD 98)

target_compile_definitions(ekalwip_static PRIVATE _GNU_SOURCE)

target_include_directories(ekalwip_static PUBLIC ${LWIP_LIB_BUILD_INCL})

set_target_properties(ekalwip_static PROPERTIES CXX_STANDARD 98)

target_compile_options(ekalwip_static PRIVATE -std=gnu++98 -fPIC)

set_target_properties(ekalwip_static PROPERTIES ARCHIVE_OUTPUT_DIRECTORY
                                                ${libDstDir})
#
# add_custom_target(lwipObj COMMAND ar x ${SMARTNIC_LIB_STATIC} COMMAND ar rcsv
# libekalwip_static.a ${lwipnoapps_SRCS}.o WORKING_DIRECTORY ${BUILD_DIR}
# DEPENDS ${SMARTNIC_LIB_STATIC} ekalwip )
#
# ##############################################################################

# ##############################################################################

file(GLOB ${EKA_LIB_NAME}_SRCS ${EKA_SRC_DIR}/*.c ${EKA_SRC_DIR}/Ehp/*.c)

set(EKA_INC_PATH ${EKA_INC_PATH} ${apiDIR})
set(EKA_INC_PATH ${EKA_INC_PATH} ${EKA_SRC_DIR})
set(EKA_INC_PATH ${EKA_INC_PATH} ${EKA_SRC_DIR}/arch)
set(EKA_INC_PATH ${EKA_INC_PATH} ${EKA_SRC_DIR}/compat)
set(EKA_INC_PATH ${EKA_INC_PATH} ${EKA_SRC_DIR}/Ehp)
set(EKA_INC_PATH ${EKA_INC_PATH} ${EKA_SRC_DIR}/FhBook)
set(EKA_INC_PATH ${EKA_INC_PATH} ${EKA_SRC_DIR}/../new_api_tests)
set(EKA_INC_PATH ${EKA_INC_PATH} ${SMARTNIC_API_INCL})
set(EKA_INC_PATH ${EKA_INC_PATH} ${SMARTNIC_DRIVER_INCL})
set(EKA_INC_PATH ${EKA_INC_PATH} ${LWIP_INCL})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ##############################################################################
# lib${EKA_LIB_NAME}.so

add_library(${EKA_LIB_NAME} SHARED ${${EKA_LIB_NAME}_SRCS})

target_compile_options(${EKA_LIB_NAME} PUBLIC -std=c++23)

target_include_directories(${EKA_LIB_NAME} PUBLIC ${EKA_INC_PATH})

target_compile_definitions(${EKA_LIB_NAME} PUBLIC LIBEKA_GIT_VER="DUMMY")

target_link_libraries(${EKA_LIB_NAME} ${SMARTNIC_LIB_STATIC} ekalwip_static rt
                      m Threads::Threads)
target_link_libraries(${EKA_LIB_NAME} -static-libgcc -static-libstdc++)

set_target_properties(${EKA_LIB_NAME} PROPERTIES PUBLIC_HEADER ${apiDIR}/*.h)

set_target_properties(${EKA_LIB_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                                                 ${libDstDir})

# ##############################################################################
# lib${EKA_LIB_NAME}_static.a

add_library(${EKA_LIB_NAME}_static STATIC ${${EKA_LIB_NAME}_SRCS})

target_compile_options(${EKA_LIB_NAME}_static PUBLIC -std=c++23)

target_include_directories(${EKA_LIB_NAME}_static PUBLIC ${EKA_INC_PATH})

target_compile_definitions(${EKA_LIB_NAME}_static PUBLIC LIBEKA_GIT_VER="DUMMY")

target_link_libraries(${EKA_LIB_NAME}_static ${SMARTNIC_LIB_STATIC}
                      ekalwip_static rt m Threads::Threads)
target_link_libraries(${EKA_LIB_NAME}_static -static-libgcc -static-libstdc++)

set_target_properties(${EKA_LIB_NAME}_static PROPERTIES ARCHIVE_OUTPUT_DIRECTORY
                                                        ${libDstDir})

# set_target_properties(${EKA_LIB_NAME}_static PROPERTIES PUBLIC_HEADER
# ${apiDIR}/*.h)

# ##############################################################################
# Tests

file(GLOB test_SRCS ${EKA_SRC_DIR}/../new_api_tests/*.c)
set(testDstDir ${EKA_SRC_DIR}/../${EKA_LIB_NAME}_tests)
file(MAKE_DIRECTORY ${testDstDir})

foreach(test ${test_SRCS})
  get_filename_component(testName ${test} NAME_WE)
  add_executable(${testName} ${test})
  set_target_properties(${testName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                               ${testDstDir})
  add_dependencies(${testName} ${EKA_LIB_NAME})
  target_link_libraries(${testName} ${EKA_LIB_NAME} rt m Threads::Threads)
  target_link_libraries(${testName} -static-libgcc -static-libstdc++)

  target_compile_options(${testName} PUBLIC -std=c++23)

  target_include_directories(${testName} PUBLIC ${EKA_INC_PATH})
endforeach()

# ##############################################################################
# Utils

file(GLOB utils_SRCS ${EKA_SRC_DIR}/../eka_utils/*.c)
set(utilsDstDir ${EKA_SRC_DIR}/../${EKA_LIB_NAME}_utils)
file(MAKE_DIRECTORY ${utilsDstDir})

foreach(uSrc ${utils_SRCS})
  get_filename_component(utilExec ${uSrc} NAME_WE)
  add_executable(${utilExec} ${uSrc})
  set_target_properties(${utilExec} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                               ${utilsDstDir})
  target_link_libraries(${utilExec} ${EKA_LIB_NAME}_static rt m
                        Threads::Threads)
  target_link_libraries(${utilExec} -static-libgcc -static-libstdc++)
  target_compile_options(${utilExec} PUBLIC -std=c++23)

  target_include_directories(${utilExec} PUBLIC ${EKA_INC_PATH})
endforeach()
