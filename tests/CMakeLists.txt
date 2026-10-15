# No test framework yet: one plain executable per file, run through CTest.
# Add a test by adding its name (without .cpp) to this list.
set(OTTER_TESTS
    test_core
)

foreach(t IN LISTS OTTER_TESTS)
    add_executable(${t} ${t}.cpp)
    target_link_libraries(${t} PRIVATE otter)
    if(MSVC)
        target_compile_options(${t} PRIVATE /W4 /utf-8)
    endif()
    add_test(NAME ${t} COMMAND ${t})
endforeach()
