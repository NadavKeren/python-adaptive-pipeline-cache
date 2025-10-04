cmake_minimum_required(VERSION 3.31)
project(pipeline_cache)

find_package(pybind11 REQUIRED)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(XXHASH_BUILD_XXHSUM OFF)        # Don't build command line tool of xxhash
option(BUILD_SHARED_LIBS OFF)       # Build static library of xxHash
add_subdirectory(external/xxHash/build/cmake xxhash_build EXCLUDE_FROM_ALL)
set_target_properties(xxhash PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(_adaptive_pipeline_cache_impl MODULE
    src/adaptive_pipeline_cache.cpp
    src/cost_aware_lfu_block.cpp
    src/approximate_lru_block.cpp
    src/fifo_block.cpp
    src/pipeline_cache.cpp
    src/count_min_sketch.cpp
)

target_link_libraries(_adaptive_pipeline_cache_impl PRIVATE pybind11::module xxHash::xxhash)
target_include_directories(_adaptive_pipeline_cache_impl PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(_adaptive_pipeline_cache_impl PRIVATE pybind11::module xxHash::xxhash)