# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14 CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE STRING "" FORCE)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

enable_testing()

include(GoogleTest)

add_executable( scanner_test
    scanner_test.cc
)

target_link_libraries( scanner_test
    PRIVATE
        GTest::gtest_main
        scan
)

target_include_directories( scanner_test
    PRIVATE
        ${LIBJAYVM_INCLUDE_DIR}
        ${LIBJAYVM_SRC_DIR}
)

add_executable( parser_test
    parser_test.cc
)

target_include_directories( parser_test
    PRIVATE
        ${LIBJAYVM_INCLUDE_DIR}
        ${LIBJAYVM_SRC_DIR}
)

target_link_libraries( parser_test
    PRIVATE
        GTest::gtest_main
        parser
)

add_executable( vector_test
    vector_test.cc
)

target_include_directories( vector_test
    PRIVATE
        ${LIBJAYVM_INCLUDE_DIR}
        ${LIBJAYVM_SRC_DIR}
)

target_link_libraries( vector_test
    PRIVATE
        GTest::gtest_main
)

gtest_discover_tests(scanner_test)
gtest_discover_tests(parser_test)
gtest_discover_tests(vector_test)