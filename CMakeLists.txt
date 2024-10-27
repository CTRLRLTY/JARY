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

cmake_minimum_required( VERSION 3.22 )

project( jary )

option( SCRUTINY "Use analyzer" OFF )
option( SANITIZE "Use asan" OFF )
option( BUILD_TEST "Build tests" OFF )
option( BUILD_TOOLS "Build tools" OFF )

find_package( SQLite3 3.4 REQUIRED )

set( CMAKE_C_STANDARD 17 CACHE STRING "Just a baseline I chose on a whim" FORCE )
set( CMAKE_C_STANDARD_REQUIRED ON CACHE STRING "Just a baseline I chose on a whim" FORCE )

if( CMAKE_C_COMPILER_ID MATCHES "^(Clang|GNU)$" )
        add_compile_options( -Wall -Wextra )
        add_compile_options( -pedantic-errors )

        if ( CMAKE_BUILD_TYPE STREQUAL Debug )
                add_compile_options( -gdwarf -g3 )
        endif()
endif()

add_library( scanner OBJECT )
add_library( parser STATIC )
add_library( compiler STATIC )
add_library( exec OBJECT )
add_library( jary SHARED )

add_subdirectory( lib/jay )

if ( BUILD_TOOLS )
        add_subdirectory( tool )
endif()

if ( BUILD_TEST )
        add_subdirectory( test )
endif()
