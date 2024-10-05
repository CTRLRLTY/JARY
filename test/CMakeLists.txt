# GoogleTest requires at least C++14
set( CMAKE_CXX_STANDARD 14 CACHE STRING "Needed by gtest" FORCE )
set( CMAKE_CXX_STANDARD_REQUIRED ON CACHE STRING "Needed by gtest" FORCE )

include( FetchContent )
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
        DOWNLOAD_EXTRACT_TIMESTAMP true
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set( gtest_force_shared_crt ON CACHE BOOL "" FORCE )

FetchContent_MakeAvailable( googletest )

enable_testing()

add_executable( scanner_test scanner_test.cc )
add_executable( parser_test parser_test.cc )
add_executable( compiler_test compiler_test.cc )
add_executable( exec_test exec_test.cc )

target_compile_options( scanner_test PRIVATE -Wno-write-strings -Wno-pedantic -fpermissive )
target_compile_options( parser_test PRIVATE -Wno-write-strings -Wno-pedantic -fpermissive )
target_compile_options( compiler_test PRIVATE -Wno-write-strings -Wno-pedantic -fpermissive )
target_compile_options( exec_test PRIVATE -Wno-write-strings -Wno-pedantic -fpermissive )

target_link_libraries( scanner_test
        PRIVATE
        GTest::gtest_main
        scanner
)

target_link_libraries( parser_test 
        PRIVATE
        GTest::gtest_main
        parser 
)

target_link_libraries( compiler_test 
        PRIVATE
        GTest::gtest_main
        compiler
)

target_link_libraries( exec_test 
        PRIVATE
        GTest::gtest_main
        exec 
)

include( GoogleTest )

gtest_discover_tests( scanner_test )
gtest_discover_tests( parser_test )
gtest_discover_tests( compiler_test )
gtest_discover_tests( exec_test )




