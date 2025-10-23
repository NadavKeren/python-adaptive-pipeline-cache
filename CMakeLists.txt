cmake_minimum_required(VERSION 3.15)
project(pipeline_cache)

# Set default build type to RelWithDebInfo if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

find_package(pybind11 REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use FetchContent to automatically download xxHash if not present
include(FetchContent)
set(XXHASH_BUILD_XXHSUM OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    xxHash
    GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
    GIT_TAG v0.8.2
    SOURCE_SUBDIR cmake_unofficial
)

FetchContent_MakeAvailable(xxHash)
set_target_properties(xxhash PROPERTIES POSITION_INDEPENDENT_CODE ON)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(json)

FetchContent_Declare(
    libassert
    GIT_REPOSITORY https://github.com/jeremy-rifkin/libassert.git
    GIT_TAG v2.1.0
)

FetchContent_MakeAvailable(libassert)

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
    PRIVATE xxHash::xxhash nlohmann_json::nlohmann_json assert
)

pybind11_add_module(_adaptive_pipeline_cache_impl
    src/adaptive_pipeline_cache_binding.cpp
)

target_link_libraries(_adaptive_pipeline_cache_impl 
    PRIVATE pipeline_cache_core
)

target_link_libraries(_adaptive_pipeline_cache_impl PUBLIC xxHash::xxhash)
target_include_directories(_adaptive_pipeline_cache_impl PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Only build tests if explicitly requested (not during wheel build)
if(BUILD_TESTING)
    add_subdirectory(tests/cpp-tests)
endif()

install(TARGETS _adaptive_pipeline_cache_impl DESTINATION adaptive_pipeline)
