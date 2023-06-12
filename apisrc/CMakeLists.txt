cmake_minimum_required(VERSION 3.0.0)
project(EkaNEW VERSION 0.1.0 LANGUAGES C CXX)

#include(CTest)
#enable_testing()

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

execute_process(COMMAND git rev-parse --short HEAD OUTPUT_VARIABLE EKA_GIT_VER)

#execute_process(COMMAND pwd OUTPUT_VARIABLE BUILD_DIR)
set(BUILD_DIR /home/vitaly/libeka/apisrc/build)

###########################################################
set(SMARTNIC_ROOT ../SmartNIC_SW)
set(SMARTNIC_API_INCL ${SMARTNIC_ROOT}/include)
set(SMARTNIC_DRIVER_INCL ${SMARTNIC_ROOT}/driver)
set(SMARTNIC_LIB ../../SmartNIC_SW/lib/libsmartnic.a)

execute_process(COMMAND ar x ${SMARTNIC_LIB})
execute_process(COMMAND ar t ${SMARTNIC_LIB} OUTPUT_VARIABLE SMARTNIC_LIB_FILES)

###########################################################

set(LWIP_DIR ../LWIP)
set(LWIP_INCL ${LWIP_DIR}/src/include)

###########################################################

set(CMAKE_C_COMPILER gcc)

include(${LWIP_DIR}/src/Filelists.cmake)

###########################################################
set(CMAKE_C_COMPILER g++)

set(ekaN_COMPILE_FLAGS -std=c++23 -O3)
set(ekaN_COMPILE_FLAGS ${ekaN_COMPILE_FLAGS} -fPIC)
set(ekaN_COMPILE_FLAGS ${ekaN_COMPILE_FLAGS} -pthread -lrt -lm)
set(ekaN_COMPILE_FLAGS ${ekaN_COMPILE_FLAGS} -static-libstdc++ -static-libgcc)
###########################################################

set(EKA_SRC_DIR .)
FILE(GLOB ekaN_SRCS ${EKA_SRC_DIR}/*.c ${EKA_SRC_DIR}/Ehp/*.c)

add_library(ekaN STATIC ${ekaN_SRCS} ${lwipnoapps_SRCS})

add_custom_target(archObj
        COMMAND ar x ${SMARTNIC_LIB}
        COMMAND ar rcsv libekaN.a ./*.o
#        WORKING_DIRECTORY ${BUILD_DIR}
        DEPENDS ${SMARTNIC_LIB}
        )

target_compile_options(ekaN PRIVATE ${ekaN_COMPILE_FLAGS})
target_include_directories(ekaN PRIVATE .)
target_include_directories(ekaN PRIVATE ./compat)
target_include_directories(ekaN PRIVATE ./Ehp)
target_include_directories(ekaN PRIVATE ./FhBook)
target_include_directories(ekaN PRIVATE ./../new_api_tests)
target_include_directories(ekaN PRIVATE ${SMARTNIC_API_INCL})
target_include_directories(ekaN PRIVATE ${SMARTNIC_DRIVER_INCL})
target_include_directories(ekaN PRIVATE ${LWIP_INCL})
#target_compile_definitions(ekaN PRIVATE LIBEKA_GIT_VER="\"${EKA_GIT_VER}\"")
#"\"${CMAKE_SOURCE_DIR}LocalBin\\example.tlb\""

#target_compile_definitions(ekaN PRIVATE LIBEKA_GIT_VER="${EKA_GIT_VER}")

#target_compile_definitions(ekaN PRIVATE LIBEKA_GIT_VER=\"${EKA_GIT_VER}\")
target_compile_definitions(ekaN PRIVATE LIBEKA_GIT_VER="abcd")

add_executable(efcCboeTest ../new_api_tests/efcCboeTest.c) 
target_include_directories(efcCboeTest PRIVATE .)
target_include_directories(efcCboeTest PRIVATE ./compat)
target_include_directories(efcCboeTest PRIVATE ./Ehp)
target_include_directories(efcCboeTest PRIVATE ./FhBook)
target_include_directories(efcCboeTest PRIVATE ./../new_api_tests)
target_include_directories(efcCboeTest PRIVATE ${SMARTNIC_API_INCL})
target_include_directories(efcCboeTest PRIVATE ${SMARTNIC_DRIVER_INCL})
target_include_directories(efcCboeTest PRIVATE ${LWIP_INCL})


add_dependencies(ekaN archObj)
#target_link_libraries(efcCboeTest ${EKA_SRC_DIR}/build/libekaN.a)
target_link_libraries(efcCboeTest ${BUILD_DIR}/libekaN.a)