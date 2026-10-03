cmake_minimum_required(VERSION 3.12...3.31)

project(
    zlib_find_package_test
    LANGUAGES C
    VERSION 1.3.2.1)

option(ZLIB_BUILD_SHARED "" ON)
option(ZLIB_BUILD_STATIC "" ON)

find_package(ZLIB 1.3.2.1 CONFIG COMPONENTS wrong REQUIRED)

if(ZLIB_BUILD_SHARED)
    add_executable(test_example /sdcard/Kad/submodules/zlib/test/example.c)
    target_link_libraries(test_example ZLIB::ZLIB)

    if(NOT ${CMAKE_SHARED_LIBRARY_SUFFIX} STREQUAL ".dll")
        add_test(NAME zlib_test_example_shared COMMAND test_example)
    endif(NOT ${CMAKE_SHARED_LIBRARY_SUFFIX} STREQUAL ".dll")
endif(ZLIB_BUILD_SHARED)

if(ZLIB_BUILD_STATIC)
    add_executable(test_example_static /sdcard/Kad/submodules/zlib/test/example.c)
    target_link_libraries(test_example_static ZLIB::ZLIBSTATIC)
    add_test(NAME zlib_test_example_static COMMAND test_example_static)
endif(ZLIB_BUILD_STATIC)
