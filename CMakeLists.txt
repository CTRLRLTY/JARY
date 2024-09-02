cmake_minimum_required(VERSION 3.22)

project(jary)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED on)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

add_library(dload OBJECT ${CMAKE_SOURCE_DIR}/lib/jay/dload.c)

if(CMAKE_C_COMPILER_ID MATCHES "^(Clang|GNU)$")
	add_compile_options(-Wall -Wextra -Wpedantic)

	if (CMAKE_BUILD_TYPE STREQUAL Debug)
		add_compile_options(-gdwarf -g3)
	endif()

	if(SANITIZE)
		message(STATUS "Using ASAN")
		add_link_options(-fsanitize=address -fno-omit-frame-pointer)
	endif()

	if(SCRUTINY)
		message(STATUS "Using Analyzer")
		add_compile_options(-fanalyzer -Wno-analyzer-malloc-leak)
	endif()

	add_compile_options($<$<COMPILE_LANGUAGE:CXX>:"-Wno-write-strings -fpermissive">)

	target_link_options(dload INTERFACE "-rdynamic")
endif()

target_include_directories(dload PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_subdirectory(${CMAKE_SOURCE_DIR}/lib/jay/modules ${CMAKE_BINARY_DIR}/modules)
# add_subdirectory(test)
add_subdirectory(tool)