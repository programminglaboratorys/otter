cmake_minimum_required(VERSION 3.15)
project(otter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# build tests/examples only when otter is the top-level project, so it can be
# dropped into another project with add_subdirectory without side effects.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(OTTER_IS_TOP_LEVEL ON)
else()
    set(OTTER_IS_TOP_LEVEL OFF)
endif()

option(OTTER_BUILD_TESTS    "Build otter tests"    ${OTTER_IS_TOP_LEVEL})
option(OTTER_BUILD_EXAMPLES "Build otter examples" ${OTTER_IS_TOP_LEVEL})


add_library(otter STATIC
    src/otter.cpp
    src/utils.cpp
)
add_library(otter::otter ALIAS otter)

target_include_directories(otter
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(otter PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(otter PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(otter PRIVATE -Wall -Wextra -pedantic)
    find_package(Threads REQUIRED)
    target_link_libraries(otter PRIVATE Threads::Threads)
endif()

# tests / examples 
if(OTTER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(OTTER_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# install
include(GNUInstallDirs)
install(TARGETS otter ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
