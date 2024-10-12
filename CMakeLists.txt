cmake_minimum_required( VERSION 3.22 )

project( jary )

option( SCRUTINY "Use analyzer" OFF )
option( SANITIZE "Use asan" OFF )
option( BUILD_TEST "Build tests" OFF )
option( BUILD_TOOLS "Build Tools" OFF )

find_package( SQLite3 3.4 REQUIRED )

set( CMAKE_C_STANDARD 17 CACHE STRING "Just a baseline I chose on a whim" FORCE )
set( CMAKE_C_STANDARD_REQUIRED on CACHE STRING "Just a baseline I chose on a whim" FORCE )
set( CMAKE_EXPORT_COMPILE_COMMANDS on )

if( CMAKE_C_COMPILER_ID MATCHES "^(Clang|GNU)$" )
        add_compile_options( -Wall -Wextra )
        add_compile_options( -pedantic-errors )

        if ( CMAKE_BUILD_TYPE STREQUAL Debug )
                add_compile_options( -gdwarf -g3 )
        endif()
endif()

add_library( dload OBJECT )
add_library( scanner OBJECT )
add_library( parser STATIC )
add_library( compiler STATIC )
add_library( exec OBJECT )
add_library( storage OBJECT )

add_subdirectory( lib/jay )

if ( BUILD_TOOLS )
        add_subdirectory( tool )
endif()

if ( BUILD_TEST )
        add_subdirectory( test )
endif()
