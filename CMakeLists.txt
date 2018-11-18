cmake_minimum_required(VERSION 3.2)
project(cnet)

###############################################################
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    exec_program(gcc ARGS "--version | grep '^gcc'|awk '{print $3}' | sed s'/)//g' | sed s'/-.*//g'" OUTPUT_VARIABLE version)
    message(STATUS "This is gcc version:: " ${version})
    if (version LESS 5.4.0)
        message(STATUS "gcc is too old")
        stop()
    elseif (version LESS 7.1.0)
        set(CMAKE_CXX_STD_FLAGS c++14)
        message(STATUS "c++14")
    else ()
        set(CMAKE_CXX_STD_FLAGS c++17)
        set(DEFS "USE_STD_ANY")
        set(USE_STD_ANY 1)
        message(STATUS "c++17")
    endif ()
else ()
    set(DEFS "USE_STD_ANY")
    set(USE_STD_ANY 1)
    set(CMAKE_CXX_STD_FLAGS c++17)
    message(STATUS "c++17")
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE Release)
endif ()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -std=${CMAKE_CXX_STD_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -std=${CMAKE_CXX_STD_FLAGS}")

###############################################################
# include directories
include_directories(
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
SET(CONFIG_HEADER "${PROJECT_SOURCE_DIR}/include/cnet/config.h")
file(WRITE "${CONFIG_HEADER}" "#pragma once\n")
foreach (loop_var ${DEFS})
    file(APPEND "${CONFIG_HEADER}" "#define ${loop_var}\n")
endforeach ()
file(APPEND "${CONFIG_HEADER}" "\n")

if (DEFINED USE_STD_ANY)
    file(APPEND "${CONFIG_HEADER}" "#include <any>\n")
    file(APPEND "${CONFIG_HEADER}" "using std::any;\n")
    file(APPEND "${CONFIG_HEADER}" "using std::any_cast;\n\n")
else ()
    file(APPEND "${CONFIG_HEADER}" "#include <experimental/any>\n")
    file(APPEND "${CONFIG_HEADER}" "using std::experimental::any;\n")
    file(APPEND "${CONFIG_HEADER}" "using std::experimental::any_cast;\n\n")
endif ()

###############################################################
aux_source_directory(src/tcp DIR_SRC)
aux_source_directory(src/http DIR_SRC)
aux_source_directory(src/utils DIR_SRC)

add_library(${PROJECT_NAME} SHARED
        ${DIR_SRC}
        src/tcp/poller/PollPoller.cc
        )
###############################################################
#add_executable(http_server
#        examples/HttpMain.cc
#        )

#target_link_libraries(http_server ${PROJECT_NAME})
