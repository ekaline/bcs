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
    "-O3
     -funroll-loops
     -pipe
     -march=native
    ")

# ##############################################################################
set(EKA_LIB_NAME ekaBC)

set(BUILD_DIR ${CMAKE_CURRENT_LIST_DIR}/build)
set(EKA_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/apisrc)
set(EKA_BC_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/bc_src)

set(libDstDir ${EKA_SRC_DIR}/../${EKA_LIB_NAME}_lib)
file(MAKE_DIRECTORY ${libDstDir})

set(apiDIR ${EKA_SRC_DIR}/../bc_include)
set(testDIR ${EKA_SRC_DIR}/../bc_tests)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ##############################################################################
set(SMARTNIC_ROOT ${CMAKE_CURRENT_LIST_DIR}/SmartNIC_SW)
set(SMARTNIC_API_INCL ${SMARTNIC_ROOT}/include)
set(SMARTNIC_DRIVER_INCL ${SMARTNIC_ROOT}/driver)
set(SMARTNIC_LIB_STATIC ${SMARTNIC_ROOT}/lib/libsmartnic.a)

# execute_process(COMMAND ar x ${SMARTNIC_LIB_STATIC}) execute_process(COMMAND
# ar t ${SMARTNIC_LIB_STATIC} OUTPUT_VARIABLE SMARTNIC_LIB_STATIC_FILES)

# ##############################################################################
# LWIP OBJS
set(LWIP_DIR ${CMAKE_CURRENT_LIST_DIR}/LWIP)
include(${LWIP_DIR}/src/Filelists.cmake)

set(ekalwip_SRCS ${lwipcore_SRCS} ${lwipcore4_SRCS} ${lwipapi_SRCS}
                 ${LWIP_DIR}/src/netif/ethernet.c)

set(LWIP_LIB_BUILD_INCL ${LWIP_LIB_BUILD_INCL} ${LWIP_DIR}/src/include)
set(LWIP_LIB_BUILD_INCL ${LWIP_LIB_BUILD_INCL} ${LWIP_DIR}/src/include/arch)
set(LWIP_LIB_BUILD_INCL ${LWIP_LIB_BUILD_INCL} ${EKA_SRC_DIR})

set(LWIP_API_INCL ${LWIP_DIR}/src/include)

foreach(lwipSrcFile ${ekalwip_SRCS})
  get_filename_component(lwipFileName ${lwipSrcFile} NAME_WE)
  add_library(${lwipFileName}.o OBJECT ${lwipSrcFile})
  target_compile_options(${lwipFileName}.o PRIVATE -std=gnu++98 -fPIC)
  target_compile_definitions(${lwipFileName}.o PRIVATE _GNU_SOURCE)
  target_link_libraries(${lwipFileName}.o rt m Threads::Threads)

  target_include_directories(${lwipFileName}.o PUBLIC ${LWIP_LIB_BUILD_INCL})

  set(LWIP_OBJS ${LWIP_OBJS} ${lwipFileName}.o)

endforeach()

message(STATUS "LWIP_OBJ_FILES == ${LWIP_OBJ_FILES}")
# ##############################################################################
# SmartNIC OBJS

set(SMARTNIC_OBJS smartnic.o smartnic_arp.o smartnic_utils.o tinythread.o)
execute_process(COMMAND ar x ${SMARTNIC_LIB_STATIC})

# ##############################################################################
# EKA OBJS

# set(EKA_SRCS ${EKA_BC_SRC_DIR}/*.c ${EKA_SRC_DIR}/*.c ${EKA_SRC_DIR}/Ehp/*.c)
file(GLOB EKA_SRCS ${EKA_BC_SRC_DIR}/*.c ${EKA_SRC_DIR}/*.c
     ${EKA_SRC_DIR}/Ehp/*.c)

set(EKA_INC_PATH
    ${apiDIR}
    ${EKA_SRC_DIR}
    ${EKA_SRC_DIR}/compat
    ${EKA_SRC_DIR}/arch
    ${EKA_SRC_DIR}/Ehp
    ${EKA_SRC_DIR}/FhBook
    ${EKA_SRC_DIR}/../new_api_tests
    ${testDir}
    ${SMARTNIC_API_INCL}
    ${SMARTNIC_DRIVER_INCL}
    ${LWIP_INCL})

foreach(ekaSrcFile ${EKA_SRCS})
  get_filename_component(ekaFileName ${ekaSrcFile} NAME_WE)
  add_library(${ekaFileName}.o OBJECT ${ekaSrcFile})
  target_compile_options(${ekaFileName}.o PRIVATE -std=c++23 -fPIC)
  target_link_libraries(${ekaFileName}.o rt m Threads::Threads)
  target_compile_definitions(${ekaFileName}.o PUBLIC LIBEKA_GIT_VER="DUMMY")
  target_include_directories(${ekaFileName}.o PUBLIC ${EKA_INC_PATH})
  target_include_directories(${ekaFileName}.o PUBLIC ${LWIP_LIB_BUILD_INCL})

  set(EKA_OBJS ${EKA_OBJS} ${ekaFileName}.o)
endforeach()

# ##############################################################################
# lib${EKA_LIB_NAME}_static.a add_custom_target(ekaStatic COMMAND ar rcsv
# lib${EKA_LIB_NAME}_static.a ${SMARTNIC_OBJS} ${LWIP_OBJS} ${EKA_OBJS} DEPENDS
# ${SMARTNIC_LIB_STATIC} ${SMARTNIC_OBJS} ${LWIP_OBJS} ${EKA_OBJS})

add_library(${EKA_LIB_NAME}_static STATIC)
target_link_libraries(${EKA_LIB_NAME}_static PUPLIC ${SMARTNIC_OBJS}
                      ${LWIP_OBJS} ${EKA_OBJS})

# add_library(${EKA_LIB_NAME} SHARED ${EKA_SRCS})
# target_link_libraries(${EKA_LIB_NAME} PUBLIC ${EKA_LIB_NAME}_static)

# ##############################################################################
# lib${EKA_LIB_NAME}.so

# add_library(${EKA_LIB_NAME} SHARED) target_link_libraries(${EKA_LIB_NAME}
# PUPLIC ${SMARTNIC_OBJS} ${LWIP_OBJS} ${EKA_OBJS})

#[==[
add_library(${EKA_LIB_NAME} SHARED ${${EKA_LIB_NAME}_SRCS})

target_compile_options(${EKA_LIB_NAME} PUBLIC -std=c++23 -fPIC)

target_include_directories(${EKA_LIB_NAME} PUBLIC ${EKA_INC_PATH})

target_compile_definitions(${EKA_LIB_NAME} PUBLIC LIBEKA_GIT_VER="DUMMY")

target_link_libraries(
  ${EKA_LIB_NAME}
  PUBLIC ${SMARTNIC_LIB_STATIC}
         ekalwip_static
         rt
         m
         Threads::Threads
         -static-libgcc
         -static-libstdc++)

set_target_properties(${EKA_LIB_NAME} PROPERTIES PUBLIC_HEADER ${apiDIR}/*.h)

set_target_properties(${EKA_LIB_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                                                 ${libDstDir})

# ##############################################################################
# lib${EKA_LIB_NAME}_static.a

add_library(${EKA_LIB_NAME}_static STATIC ${${EKA_LIB_NAME}_SRCS})

target_compile_options(${EKA_LIB_NAME}_static PUBLIC -std=c++23 -fPIC)

target_include_directories(${EKA_LIB_NAME}_static PUBLIC ${EKA_INC_PATH})

target_compile_definitions(${EKA_LIB_NAME}_static PUBLIC LIBEKA_GIT_VER="DUMMY")

target_link_libraries(
  ${EKA_LIB_NAME}_static
  PUBLIC ${SMARTNIC_LIB_STATIC}
         ekalwip_static
         rt
         m
         Threads::Threads
         -static-libgcc
         -static-libstdc++)

set_target_properties(${EKA_LIB_NAME}_static PROPERTIES ARCHIVE_OUTPUT_DIRECTORY
                                                        ${libDstDir})

# set_target_properties(${EKA_LIB_NAME}_static PROPERTIES PUBLIC_HEADER
# ${apiDIR}/*.h)

# ##############################################################################
# Tests

file(GLOB test_SRCS ${EKA_SRC_DIR}/../bc_tests/*.c)
set(testDstDir ${EKA_SRC_DIR}/../${EKA_LIB_NAME}_tests)
file(MAKE_DIRECTORY ${testDstDir})

foreach(test ${test_SRCS})
  get_filename_component(testName ${test} NAME_WE)
  add_executable(${testName} ${test})
  set_target_properties(${testName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                               ${testDstDir})
  add_dependencies(${testName} ${EKA_LIB_NAME})
  target_link_libraries(
    ${testName}
    ${EKA_LIB_NAME}
    rt
    m
    Threads::Threads
    -static-libgcc
    -static-libstdc++)

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
]==]
