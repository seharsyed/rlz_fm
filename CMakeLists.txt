cmake_minimum_required (VERSION 3.15)
project (rlz)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(VERSION_MAJOR "1")
SET(VERSION_MINOR "1")
SET(VERSION_PATCH "0")
SET(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

###############################################################################
# Set install directory if not specified
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE PATH "Installation directory")
################################################################################
# Specify build type
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install directory: ${CMAKE_INSTALL_PREFIX}")
################################################################################
# Locating Git
find_package(Git)
if(GIT_FOUND)
    message(STATUS "git found: ${GIT_EXECUTABLE}")
else()
    message(FATAL_ERROR "git not found. Cloning of submodules will not work.")
endif()
################################################################################
# Version number of project
execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE)

message(STATUS "Git current branch of project: ${GIT_BRANCH}")
message(STATUS "Git commit hash of project: ${GIT_COMMIT_HASH}")
################################################################################
# Compiler and linker flags
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(BUILD_TYPE_FLAGS "-O3")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_TYPE_FLAGS "-g")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(BUILD_TYPE_FLAGS "-O3 -g -DNDEBUG")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${BUILD_TYPE_FLAGS}")
#################################################################################
# Check dependencies
message(STATUS "The source directory of the project is ${CMAKE_CURRENT_LIST_DIR}")

set(PROJECT_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(THIRDPARTY_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/thirdparty)

message(STATUS "The include directories of the project is ${INCLUDE_DIRECTORIES}")

make_directory(${CMAKE_CURRENT_BINARY_DIR}/logs)
set(INSTALL_FILE  ${CMAKE_CURRENT_BINARY_DIR}/install.cmake)

# OpenMP
find_package(OpenMP)
if(OPENMP_FOUND)
    include_directories(${OpenMP_CXX_INCLUDE_DIRS})
    link_libraries(${OpenMP_CXX_LIBRARIES})
    if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS} -fopenmp")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS} -fopenmp")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    endif()
endif()

# DIVSUFSORT
find_library(DIVSUFSORT_LIB divsufsort HINTS ${CMAKE_CURRENT_BINARY_DIR}/lib ${CMAKE_CURRENT_BINARY_DIR}/sdsl/lib PATHS ${CONDA_PREFIX}/lib)
if(NOT DIVSUFSORT_LIB)
    message(STATUS "divsufsort library not found. Building from submodule...")
else()
    message(STATUS "divsufsort library found at ${DIVSUFSORT_LIB}.")
endif()

# DIVSUFSORT64
find_library(DIVSUFSORT64_LIB divsufsort64 HINTS ${CMAKE_CURRENT_BINARY_DIR}/lib ${CMAKE_CURRENT_BINARY_DIR}/sdsl/lib PATHS ${CONDA_PREFIX}/lib)
if(NOT DIVSUFSORT64_LIB)
    message(STATUS "divsufsort64 library not found. Building from submodule...")
else()
    message(STATUS "divsufsort64 library found at ${DIVSUFSORT64_LIB}.")
endif()

# SDSL
find_library(SDSL_LIB sdsl HINTS ${CMAKE_CURRENT_BINARY_DIR}/lib ${CMAKE_CURRENT_BINARY_DIR}/sdsl/lib PATHS ${CONDA_PREFIX}/lib)
find_path(SDSL_SRC sdsl HINTS ${CMAKE_CURRENT_BINARY_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/sdsl/include PATHS ${CONDA_PREFIX}/include)

if(NOT SDSL_LIB OR NOT DIVSUFSORT_LIB OR NOT DIVSUFSORT64_LIB)
    message(STATUS "sdsl library not found. Building from submodule...")

    set(sdsl_SOURCE_DIR ${THIRDPARTY_INCLUDE_DIR}/sdsl-lite)

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/sdsl/logs)

    execute_process(COMMAND ./install.sh ${CMAKE_CURRENT_BINARY_DIR}/sdsl
            RESULT_VARIABLE result
            WORKING_DIRECTORY ${sdsl_SOURCE_DIR}
            OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/sdsl/logs/sdsl.log
            ERROR_FILE ${CMAKE_CURRENT_BINARY_DIR}/sdsl/logs/sdsl.log)

    file(APPEND ${INSTALL_FILE} "MESSAGE(STATUS \"Installing sdsl.\")\n")
    file(APPEND ${INSTALL_FILE} "execute_process(COMMAND ./install.sh \${CMAKE_INSTALL_PREFIX}/sdsl
            WORKING_DIRECTORY ${sdsl_SOURCE_DIR}
            OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/sdsl/logs/sdsl.log
            ERROR_FILE ${CMAKE_CURRENT_BINARY_DIR}/sdsl/logs/sdsl.log)\n")

    if(result)
        message(FATAL_ERROR "CMake step for sdsl-lite failed: ${result}")
    endif()

    set(SDSL_SRC ${CMAKE_CURRENT_BINARY_DIR}/sdsl/include CACHE PATH "Path to include dir" FORCE)
    set(DIVSUFSORT_SRC ${CMAKE_CURRENT_BINARY_DIR}/sdsl/include CACHE PATH "Path to include dir" FORCE)
    set(SDSL_LIB ${CMAKE_CURRENT_BINARY_DIR}/sdsl/lib/libsdsl.a CACHE PATH "Path to libsdsl.a" FORCE)
    set(DIVSUFSORT_LIB ${CMAKE_CURRENT_BINARY_DIR}/sdsl/lib/libdivsufsort.a CACHE PATH "Path to libdivsufsort.a" FORCE)
    set(DIVSUFSORT64_LIB ${CMAKE_CURRENT_BINARY_DIR}/sdsl/lib/libdivsufsort64.a CACHE PATH "Path to libdivsufsort64.a" FORCE)
else()
    message(STATUS "sdsl library found at ${SDSL_LIB}.")
    message(STATUS "sdsl sources found at ${SDSL_SRC}.")
endif()

################################################################################
# Original FM baseline wrapper
add_library(fm_wrapper ${PROJECT_SRC_DIR}/fm_wrapper.cpp)
target_include_directories(fm_wrapper PUBLIC
    ${SDSL_SRC}
    ${DIVSUFSORT_SRC}
    ${DIVSUFSORT64_SRC}
    ${PROJECT_INCLUDE_DIR})
target_link_libraries(fm_wrapper PUBLIC
    ${SDSL_LIB}
    ${DIVSUFSORT_LIB}
    ${DIVSUFSORT64_LIB})

################################################################################
# Original FM cached wrapper
add_library(fm_wrapper_cached ${PROJECT_SRC_DIR}/fm_wrapper_cached.cpp)
target_include_directories(fm_wrapper_cached PUBLIC
    ${SDSL_SRC}
    ${DIVSUFSORT_SRC}
    ${DIVSUFSORT64_SRC}
    ${PROJECT_INCLUDE_DIR})
target_link_libraries(fm_wrapper_cached PUBLIC
    ${SDSL_LIB}
    ${DIVSUFSORT_LIB}
    ${DIVSUFSORT64_LIB})

################################################################################
# Original runner
add_library(rlz_list_runner ${PROJECT_SRC_DIR}/rlz_list_runner.cpp)
target_include_directories(rlz_list_runner PUBLIC
    ${PROJECT_INCLUDE_DIR}
    ${SDSL_SRC}
    ${THIRDPARTY_INCLUDE_DIR}
    ${THIRDPARTY_INCLUDE_DIR}/CLI)
target_link_libraries(rlz_list_runner PUBLIC
    ${SDSL_LIB}
    ${DIVSUFSORT_LIB}
    ${DIVSUFSORT64_LIB}
    fm_wrapper
    fm_wrapper_cached)

################################################################################
# Original executable
add_executable(rlz ${PROJECT_SRC_DIR}/rlz.cpp)
target_include_directories(rlz PUBLIC
    ${PROJECT_INCLUDE_DIR}
    ${SDSL_SRC}
    ${THIRDPARTY_INCLUDE_DIR}
    ${THIRDPARTY_INCLUDE_DIR}/CLI)
target_link_libraries(rlz PUBLIC
    ${SDSL_LIB}
    fm_wrapper
    fm_wrapper_cached
    rlz_list_runner)

################################################################################
# New FM linear probing cache
add_library(fm_lp_cache ${PROJECT_SRC_DIR}/fm_lp.cpp)
target_include_directories(fm_lp_cache PUBLIC
    ${SDSL_SRC}
    ${DIVSUFSORT_SRC}
    ${DIVSUFSORT64_SRC}
    ${PROJECT_INCLUDE_DIR})
target_link_libraries(fm_lp_cache PUBLIC
    ${SDSL_LIB}
    ${DIVSUFSORT_LIB}
    ${DIVSUFSORT64_LIB})

################################################################################
# New FM linear probing runner
add_library(rlz_list_runner_lp ${PROJECT_SRC_DIR}/rlz_list_runner_lp.cpp)
target_include_directories(rlz_list_runner_lp PUBLIC
    ${PROJECT_INCLUDE_DIR}
    ${SDSL_SRC}
    ${THIRDPARTY_INCLUDE_DIR}
    ${THIRDPARTY_INCLUDE_DIR}/CLI)
target_link_libraries(rlz_list_runner_lp PUBLIC
    ${SDSL_LIB}
    ${DIVSUFSORT_LIB}
    ${DIVSUFSORT64_LIB}
    fm_wrapper
    fm_lp_cache)

################################################################################
# New FM linear probing executable
add_executable(fm_lp ${PROJECT_SRC_DIR}/rlz.cpp)
target_include_directories(fm_lp PUBLIC
    ${PROJECT_INCLUDE_DIR}
    ${SDSL_SRC}
    ${THIRDPARTY_INCLUDE_DIR}
    ${THIRDPARTY_INCLUDE_DIR}/CLI)
target_link_libraries(fm_lp PUBLIC
    ${SDSL_LIB}
    fm_wrapper
    fm_lp_cache
    rlz_list_runner_lp)

################################################################################
# Other original tools
add_executable(sort
    ${PROJECT_SRC_DIR}/sort.cpp
    ${PROJECT_SRC_DIR}/sort_algo_text.cpp
    ${PROJECT_SRC_DIR}/benchmark_logger.cpp)
target_include_directories(sort PUBLIC
    ${PROJECT_INCLUDE_DIR}
    ${SDSL_SRC}
    ${THIRDPARTY_INCLUDE_DIR}
    ${THIRDPARTY_INCLUDE_DIR}/CLI)
target_link_libraries(sort PUBLIC
    ${SDSL_LIB}
    ${DIVSUFSORT_LIB}
    ${DIVSUFSORT64_LIB})

add_executable(stats ${PROJECT_SRC_DIR}/stats.cpp)
target_include_directories(stats PUBLIC
    ${PROJECT_INCLUDE_DIR}
    ${THIRDPARTY_INCLUDE_DIR}
    ${THIRDPARTY_INCLUDE_DIR}/CLI)

################################################################################
# Install targets
install(TARGETS fm_wrapper TYPE LIBRARY)
install(TARGETS fm_wrapper_cached TYPE LIBRARY)
install(TARGETS rlz_list_runner TYPE LIBRARY)
install(TARGETS rlz TYPE RUNTIME)
install(TARGETS fm_lp_cache TYPE LIBRARY)
install(TARGETS rlz_list_runner_lp TYPE LIBRARY)
install(TARGETS fm_lp TYPE RUNTIME)
install(TARGETS sort TYPE RUNTIME)
install(TARGETS stats TYPE RUNTIME)