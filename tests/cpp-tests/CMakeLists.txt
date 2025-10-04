set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/52eb8108c5bdec04579160ae17225d66034bd723.zip   
)

FetchContent_MakeAvailable(googletest)

enable_testing()

add_library(common_includes INTERFACE)
target_include_directories(common_includes INTERFACE ../../src)
target_link_libraries(common_includes INTERFACE GTest::gtest_main)
include(GoogleTest)

file(GLOB TEST_SUITES "*.cpp")
foreach(test_file ${TEST_SUITES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE common_includes)
    gtest_discover_tests(${test_name})
endforeach()
