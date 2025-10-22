cmake_minimum_required(VERSION 3.15)
project(pipeline_cache)
set(CMAKE_BUILD_TYPE Debug)

find_package(pybind11 REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    xxHash
    GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
    GIT_TAG v0.8.2
)

FetchContent_GetProperties(xxHash)
if(NOT xxhash_POPULATED)
    FetchContent_Populate(xxHash)
endif()

add_library(xxhash STATIC
    ${xxhash_SOURCE_DIR}/xxhash.c
)
target_include_directories(xxhash PUBLIC ${xxhash_SOURCE_DIR})
set_target_properties(xxhash PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(pipeline_cache_core STATIC
    src/adaptive_pipeline_cache.cpp
    src/cost_aware_lfu_block.cpp
    src/approximate_lru_block.cpp
    src/fifo_block.cpp
    src/pipeline_cache.cpp
    src/count_min_sketch.cpp
)

set_target_properties(pipeline_cache_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(pipeline_cache_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(pipeline_cache_core
    PRIVATE xxhash
)

pybind11_add_module(_adaptive_pipeline_cache_impl
    src/adaptive_pipeline_cache_binding.cpp
)

target_link_libraries(_adaptive_pipeline_cache_impl
    PRIVATE pipeline_cache_core xxhash
)

target_include_directories(_adaptive_pipeline_cache_impl PRIVATE ${CMAKE_SOURCE_DIR}/src)

if(BUILD_TESTING)
    add_subdirectory(tests/cpp-tests)
endif()

install(TARGETS _adaptive_pipeline_cache_impl DESTINATION .)
