cmake_minimum_required(VERSION 3.31)
project(pipeline_cache)

find_package(pybind11 REQUIRED)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(XXHASH_BUILD_XXHSUM OFF)        # Don't build command line tool of xxhash
option(BUILD_SHARED_LIBS OFF)       # Build static library of xxHash
add_subdirectory(external/xxHash/build/cmake xxhash_build EXCLUDE_FROM_ALL)

target_link_libraries(_adaptive_pipeline_cache_impl PRIVATE pybind11::module xxHash::xxhash)
target_include_directories(_adaptive_pipeline_cache_impl PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(_adaptive_pipeline_cache_impl PRIVATE pybind11::module xxHash::xxhash)