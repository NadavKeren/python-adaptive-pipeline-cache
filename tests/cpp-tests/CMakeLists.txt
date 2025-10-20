set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/52eb8108c5bdec04579160ae17225d66034bd723.zip   
)

FetchContent_MakeAvailable(googletest)

enable_testing()

# add_library(common_includes INTERFACE)
# target_include_directories(common_includes INTERFACE ../../src)
# target_link_libraries(common_includes INTERFACE GTest::gtest_main)
include(GoogleTest)

# set(XXHASH_BUILD_XXHSUM OFF)
# option(BUILD_SHARED_LIBS OFF)
# add_subdirectory(../../external/xxHash/build/cmake xxhash_build EXCLUDE_FROM_ALL)
# set_target_properties(xxhash PROPERTIES POSITION_INDEPENDENT_CODE ON)

# target_link_libraries(_adaptive_pipeline_cache_impl PRIVATE xxHash::xxhash)
# target_include_directories(_adaptive_pipeline_cache_impl PRIVATE $../../src)

file(GLOB TEST_SUITES "*.cpp")
foreach(test_file ${TEST_SUITES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE pipeline_cache_core xxhash GTest::gtest_main)
    target_compile_definitions(${test_name} PRIVATE
    "CMAKE_CURRENT_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\""
    )
    if (NOT ${test_name} STREQUAL "test_sanity_from_file")
        gtest_discover_tests(${test_name})
    endif()
endforeach()
