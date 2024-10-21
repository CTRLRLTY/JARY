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

target_sources( scanner PRIVATE scanner.c )
target_sources( parser PUBLIC memory.c PRIVATE parser.c)
target_sources( compiler PUBLIC memory.c PRIVATE compiler.c defs.c dload.c )
target_sources( exec PRIVATE exec.c storage.c )

target_sources( jary 
        PRIVATE 
        scanner.c
        memory.c
        parser.c
        compiler.c
        defs.c
        dload.c
        exec.c 
        storage.c 
        jary.c
)


if( CMAKE_C_COMPILER_ID MATCHES "^(Clang|GNU)$" )
        # to enable plugin definition in dload.c
        target_link_options( compiler INTERFACE -rdynamic )
endif()

target_include_directories( scanner 
        PUBLIC 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/lib/jay
)
target_include_directories( parser 
        PUBLIC 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/lib/jay
)
target_include_directories( compiler 
        PUBLIC 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/lib/jay
)
target_include_directories( exec 
        PUBLIC 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/lib/jay
)

target_include_directories( jary 
        PUBLIC 
        ${CMAKE_SOURCE_DIR}/include
        PRIVATE
        ${CMAKE_SOURCE_DIR}/lib/jay
)

target_link_libraries( parser PRIVATE scanner )
target_link_libraries( compiler PRIVATE parser )
target_link_libraries( exec INTERFACE compiler PRIVATE SQLite::SQLite3 )
target_link_libraries( jary PRIVATE SQLite::SQLite3 )

if ( SCRUTINY AND CMAKE_C_COMPILER_ID STREQUAL "GNU" )
        target_compile_options( parser BEFORE PRIVATE -fanalyzer )
        target_compile_options( scanner BEFORE PRIVATE -fanalyzer )
        target_compile_options( compiler BEFORE PRIVATE -fanalyzer )
        target_compile_options( exec BEFORE PRIVATE -fanalyzer )
        target_compile_options( jary BEFORE PRIVATE -fanalyzer )
endif()

add_subdirectory( modules ${CMAKE_BINARY_DIR}/modules )
