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
add_executable( storage_test storage_test.cc )

target_compile_definitions( compiler_test 
        PUBLIC 
        BASIC_JARY_PATH="$<TARGET_FILE_DIR:compiler_test>/compiler_basic.jary" 
)

target_compile_definitions( exec_test 
        PUBLIC 
        MARK_MODULE_JARY_PATH="$<TARGET_FILE_DIR:exec_test>/exec_mark_module.jary" 
)

target_compile_options( scanner_test PRIVATE -Wno-write-strings -Wno-pedantic -fpermissive )
target_compile_options( parser_test PRIVATE -Wno-write-strings -Wno-pedantic -fpermissive )
target_compile_options( compiler_test PRIVATE -Wno-write-strings -Wno-pedantic -fpermissive )
target_compile_options( exec_test PRIVATE -Wno-write-strings -Wno-pedantic -fpermissive )
# target_compile_options( storage_test PRIVATE -Wno-write-strings -Wno-pedantic -fpermissive )

if ( SANITIZE AND UNIX AND CMAKE_C_COMPILER_ID MATCHES "^(Clang|GNU)$" )
        target_link_options( exec_test BEFORE PRIVATE -fsanitize=address -fno-omit-frame-pointer )
        target_link_options( storage_test BEFORE PRIVATE -fsanitize=address -fno-omit-frame-pointer )
endif()

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

target_link_libraries( storage_test
        PRIVATE
        GTest::gtest_main
        compiler
        storage
)

add_custom_command(
        TARGET compiler_test
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/compiler_basic.jary
        $<TARGET_FILE_DIR:compiler_test>/compiler_basic.jary
)

add_custom_command(
        TARGET exec_test 
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/exec_mark_module.jary
        $<TARGET_FILE_DIR:compiler_test>/exec_mark_module.jary
)

include( GoogleTest )

gtest_discover_tests( scanner_test )
gtest_discover_tests( parser_test )
gtest_discover_tests( compiler_test )
gtest_discover_tests( exec_test )
gtest_discover_tests( storage_test )




