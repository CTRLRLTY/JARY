# BSD 3-Clause License
#
# Copyright (c) 2024. Muhammad Raznan. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
add_executable( jary_test jary_test.cc )

target_compile_definitions( compiler_test 
        PUBLIC 
        BASIC_JARY_PATH="$<TARGET_FILE_DIR:compiler_test>/compiler_basic.jary" 
)
target_compile_definitions( exec_test 
        PUBLIC 
        MARK_MODULE_JARY_PATH="$<TARGET_FILE_DIR:exec_test>/exec_mark_module.jary" 
        JOIN_JARY_PATH="$<TARGET_FILE_DIR:exec_test>/exec_join.jary" 
        WITHIN_JARY_PATH="$<TARGET_FILE_DIR:exec_test>/exec_within.jary"
        BETWEEN_JARY_PATH="$<TARGET_FILE_DIR:exec_test>/exec_between.jary"
)
target_compile_definitions( jary_test 
        PUBLIC 
        SIMPLE_JARY_PATH="$<TARGET_FILE_DIR:compiler_test>/jary_simple.jary" 
        MODULE_DIR="${CMAKE_BINARY_DIR}/modules/" 
)


target_compile_options( scanner_test 
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-write-strings -Wno-pedantic -Wno-missing-field-initializers -fpermissive>
)
target_compile_options( parser_test 
        PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-write-strings -Wno-pedantic -Wno-missing-field-initializers -fpermissive>
)
target_compile_options( compiler_test 
        PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-write-strings -Wno-pedantic -Wno-missing-field-initializers -fpermissive>
)
target_compile_options( exec_test 
        PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-write-strings -Wno-pedantic -Wno-missing-field-initializers -fpermissive>
)
target_compile_options( jary_test 
        PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-write-strings -Wno-pedantic -Wno-missing-field-initializers -fpermissive>
)

if ( SANITIZE AND UNIX AND CMAKE_C_COMPILER_ID MATCHES "^(Clang|GNU)$" )
        target_link_options( jary_test BEFORE PRIVATE -fsanitize=address -fno-omit-frame-pointer )
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

target_link_libraries( jary_test 
        PRIVATE
        GTest::gtest_main
        jary 
)

add_custom_command(
        TARGET compiler_test
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/compiler_*.jary
        $<TARGET_FILE_DIR:compiler_test>/
)

add_custom_command(
        TARGET exec_test 
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/exec_*.jary
        $<TARGET_FILE_DIR:compiler_test>/
)

add_custom_command(
        TARGET jary_test 
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/jary_*.jary
        $<TARGET_FILE_DIR:compiler_test>/
)

include( GoogleTest )

gtest_discover_tests( scanner_test )
gtest_discover_tests( parser_test )
gtest_discover_tests( compiler_test )
gtest_discover_tests( exec_test )
gtest_discover_tests( jary_test )
