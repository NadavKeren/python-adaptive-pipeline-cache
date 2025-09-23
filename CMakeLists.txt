cmake_minimum_required(VERSION 3.31)
project(pipeline_cache)

find_package(pybind11 REQUIRED)

set(CMAKE_CXX_STANDARD 17)

## Removed executable target; only building Python module
add_library(_adaptive_pipeline_cache_impl MODULE src/adaptive_pipeline_cache.cpp)
target_link_libraries(_adaptive_pipeline_cache_impl PRIVATE pybind11::module)
## Removed linking for executable target