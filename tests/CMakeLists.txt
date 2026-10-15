# Test runner: runs every case in cases.hpp against every backend.
# Add backends in test_main.cpp, cases in cases.hpp.
add_executable(ini_tests test_main.cpp)
target_link_libraries(ini_tests PRIVATE otter)
if(MSVC)
    target_compile_options(ini_tests PRIVATE /W4 /utf-8)
else()
    target_compile_options(ini_tests PRIVATE -Wall -Wextra -pedantic)
endif()
add_test(NAME ini_tests COMMAND ini_tests)
