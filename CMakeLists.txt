cmake_minimum_required(VERSION 3.22)

# -- CONFIGURATION
project(jary)

# Enable AddressSanitizer for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL Debug AND SANITIZE)
	if(CMAKE_C_COMPILER_ID MATCHES "^(Clang|GNU)$")
        	message(STATUS "Using ASAN")
        	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    	endif()
endif()

# -- DEFINITIONS
set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib)

file(GLOB LIBJAYVM_SRC_DIR ${LIB_DIR}/vm)
file(GLOB LIBJAYVM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

# -- TARGETS
add_library( scan 
    	${LIBJAYVM_SRC_DIR}/scanner.c
)

add_library( parser
    	${LIBJAYVM_SRC_DIR}/parser.c
    	${LIBJAYVM_SRC_DIR}/scanner.c
)

add_library( compiler
	${LIBJAYVM_SRC_DIR}/compiler.c
	${LIBJAYVM_SRC_DIR}/parser.c
)

target_include_directories( scan
	PRIVATE
		${LIBJAYVM_INCLUDE_DIR}
		${LIBJAYVM_SRC_DIR}
)

target_include_directories( parser
	PUBLIC
		${LIBJAYVM_INCLUDE_DIR}/jary/ast.h
		${LIBJAYVM_INCLUDE_DIR}/jary/token.h
	PRIVATE
		${LIBJAYVM_INCLUDE_DIR}
		${LIBJAYVM_SRC_DIR}
)

target_include_directories( compiler
	PRIVATE
		${LIBJAYVM_INCLUDE_DIR}
		${LIBJAYVM_SRC_DIR}
)


# add_subdirectory(test)
add_subdirectory(tool)