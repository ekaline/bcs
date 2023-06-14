cmake_minimum_required(VERSION 3.0.0)
project(EkaNEW VERSION 0.1.0 LANGUAGES C CXX)

#include(CTest)
#enable_testing()

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

execute_process(COMMAND git rev-parse --short HEAD OUTPUT_VARIABLE EKA_GIT_VER)

set(CMAKE_C_COMPILER ${CMAKE_CXX_COMPILER})

set(BUILD_DIR ${CMAKE_CURRENT_LIST_DIR}/build)

set(EKA_COMMON_CXX_FLAGS ${EKA_COMMON_CXX_FLAGS} -std=c++23)
set(EKA_COMMON_CXX_FLAGS ${EKA_COMMON_CXX_FLAGS} -O3 -funroll-loops)
set(EKA_COMMON_CXX_FLAGS ${EKA_COMMON_CXX_FLAGS} -pthread -lrt -lm)
#set(EKA_COMMON_CXX_FLAGS ${EKA_COMMON_CXX_FLAGS} -static-libstdc++ -static-libgcc)
set(EKA_COMMON_CXX_FLAGS ${EKA_COMMON_CXX_FLAGS} -I .)

set(EKA_COMMON_CXX_LIB_FLAGS ${EKA_COMMON_CXX_LIB_FLAGS} ${EKA_COMMON_CXX_FLAGS})
set(EKA_COMMON_CXX_LIB_FLAGS ${EKA_COMMON_CXX_LIB_FLAGS} -fPIC -shared)

set(EKA_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/apisrc)

###########################################################
set(SMARTNIC_ROOT ${CMAKE_CURRENT_LIST_DIR}/SmartNIC_SW)
set(SMARTNIC_API_INCL ${SMARTNIC_ROOT}/include)
set(SMARTNIC_DRIVER_INCL ${SMARTNIC_ROOT}/driver)
set(SMARTNIC_LIB ${SMARTNIC_ROOT}/lib/libsmartnic.a)

#execute_process(COMMAND ar x ${SMARTNIC_LIB})
#execute_process(COMMAND ar t ${SMARTNIC_LIB} OUTPUT_VARIABLE SMARTNIC_LIB_FILES)

###########################################################
# libekalwip.so
set(LWIP_DIR ${CMAKE_CURRENT_LIST_DIR}/LWIP)
include(${LWIP_DIR}/src/Filelists.cmake)

set(ekalwip_SRCS
    ${lwipcore_SRCS}
    ${lwipcore4_SRCS}
    ${lwipapi_SRCS}
    ${LWIP_DIR}/src/netif/ethernet.c
)

set(LWIP_LIB_BUILD_INCL ${LWIP_LIB_BUILD_INCL} ${LWIP_DIR}/src/include)
set(LWIP_LIB_BUILD_INCL ${LWIP_LIB_BUILD_INCL} ${LWIP_DIR}/src/include/arch)
set(LWIP_LIB_BUILD_INCL ${LWIP_LIB_BUILD_INCL} ${EKA_SRC_DIR})

set(LWIP_API_INCL ${LWIP_DIR}/src/include)

add_library(ekalwip SHARED ${ekalwip_SRCS})
target_compile_options(ekalwip PUBLIC
        -O3 -funroll-loops
        -fPIC -shared
        -pthread -lrt -lm
        -pipe -march=native
)
target_compile_definitions(ekalwip PRIVATE _GNU_SOURCE)

target_include_directories(ekalwip PUBLIC ${LWIP_LIB_BUILD_INCL})

#----------------------------------------------------------
# libekalwip_static.a
#[==================[
#add_custom_target(lwipObj
#        COMMAND ar x ${SMARTNIC_LIB}
#        COMMAND ar rcsv libekalwip_static.a ${lwipnoapps_SRCS}.o
#        WORKING_DIRECTORY ${BUILD_DIR}
#        DEPENDS ${SMARTNIC_LIB} ekalwip
#        )
#]==================]
###########################################################

###########################################################

FILE(GLOB ekaN_SRCS ${EKA_SRC_DIR}/*.c ${EKA_SRC_DIR}/Ehp/*.c)

set(EKA_INC_PATH ${EKA_INC_PATH} ${EKA_SRC_DIR})
set(EKA_INC_PATH ${EKA_INC_PATH} ${EKA_SRC_DIR}/arch)
set(EKA_INC_PATH ${EKA_INC_PATH} ${EKA_SRC_DIR}/compat)
set(EKA_INC_PATH ${EKA_INC_PATH} ${EKA_SRC_DIR}/Ehp)
set(EKA_INC_PATH ${EKA_INC_PATH} ${EKA_SRC_DIR}/FhBook)
set(EKA_INC_PATH ${EKA_INC_PATH} ${EKA_SRC_DIR}/../new_api_tests)
set(EKA_INC_PATH ${EKA_INC_PATH} ${SMARTNIC_API_INCL})
set(EKA_INC_PATH ${EKA_INC_PATH} ${SMARTNIC_DRIVER_INCL})
set(EKA_INC_PATH ${EKA_INC_PATH} ${LWIP_INCL})

###########################################################
# libekaN.so
add_library(ekaN SHARED ${ekaN_SRCS})

target_link_libraries (ekaN ${SMARTNIC_LIB} ekalwip)

target_compile_options(ekaN PUBLIC
        -std=c++23
        -O3 -funroll-loops
        -fPIC -shared
        -pthread -lrt -lm
        -pipe -march=native
        )

target_include_directories(ekaN PUBLIC ${EKA_INC_PATH})

target_compile_definitions(ekaN PUBLIC LIBEKA_GIT_VER="DUMMY")

target_link_options(ekaN PUBLIC
        -static-libstdc++ -static-libgcc
        -pthread -lrt -lm
)
set_target_properties(ekaN PROPERTIES PUBLIC_HEADER ${EKA_SRC_DIR}/compat/*.h)

###########################################################
# libekaN_static.a

add_custom_target(archObj
        COMMAND ar x ${SMARTNIC_LIB}
        COMMAND ar rcsv libekaN_static.a ${BUILD_DIR}/*.o
        WORKING_DIRECTORY ${BUILD_DIR}
        DEPENDS ${SMARTNIC_LIB} ekaN
        )

###########################################################
# efcCboeTest
set(tests_COMPILE_FLAGS ${tests_COMPILE_FLAGS} ${EKA_COMMON_CXX_FLAGS})

add_executable(efcCboeTest ${EKA_SRC_DIR}/../new_api_tests/efcCboeTest.c)
add_dependencies(efcCboeTest ekaN)
target_compile_options(efcCboeTest PUBLIC ${tests_COMPILE_FLAGS})

set(tests_LINK_FLAGS -std=c++23 -O3 -funroll-loops)
set(tests_LINK_FLAGS ${tests_LINK_FLAGS} -pipe -march=native)
set(tests_LINK_FLAGS ${tests_LINK_FLAGS} -pthread -lrt -lm)
set(tests_LINK_FLAGS ${tests_LINK_FLAGS} -static-libstdc++ -static-libgcc)
#set(tests_LINK_FLAGS ${tests_LINK_FLAGS} -lekaN -L${BUILD_DIR})

target_link_libraries(efcCboeTest ekaN)
target_link_options(efcCboeTest PUBLIC
        -static-libstdc++ -static-libgcc
        -pthread -lrt -lm
        )

target_include_directories(efcCboeTest PUBLIC ${EKA_INC_PATH})

###########################################################


#add_dependencies(ekaN archObj)
#target_link_libraries(efcCboeTest ${EKA_SRC_DIR}/build/libekaN.a)
