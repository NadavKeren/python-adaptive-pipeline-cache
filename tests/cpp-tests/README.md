Building the tests in this folder is done by:
>> cmake -S . -B build && cmake --build build

Then:
>> cd build

You can now run the tests using
>> ./ctest

Or run individual test with breaking on error:
>> ./test_file --gtest_catch_exceptions=0 --gtest_break_on_failure