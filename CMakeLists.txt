cmake_minimum_required(VERSION 3.22)

# -- CONFIGURATION

project(thaumiel)

# -- DEFINITIONS
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

file(GLOB LIBTVM_SRC_DIR ${SRC_DIR}/vm)
file(GLOB LIBTVM_INCLUDE_DIR ${LIBTVM_SRC_DIR}/include)

# -- TARGETS
add_library(scan 
    ${LIBTVM_SRC_DIR}/scanner.c
    ${LIBTVM_SRC_DIR}/token.c 
    ${LIBTVM_SRC_DIR}/vector.c
    ${LIBTVM_SRC_DIR}/fnv.c
)
target_include_directories(scan
    PUBLIC
        ${LIBTVM_INCLUDE_DIR}
    PRIVATE
        ${LIBTVM_SRC_DIR}
)

add_library(parser
    ${LIBTVM_SRC_DIR}/parser.c
    ${LIBTVM_SRC_DIR}/token.c 
    ${LIBTVM_SRC_DIR}/vector.c
    ${LIBTVM_SRC_DIR}/fnv.c
    ${LIBTVM_SRC_DIR}/ast.c
)
target_include_directories(parser
    PUBLIC
        ${LIBTVM_INCLUDE_DIR}
    PRIVATE
        ${LIBTVM_SRC_DIR}
)

add_library(vm ${LIBTVM_SRC_DIR}/vm.c)
target_include_directories(vm 
    PUBLIC
        ${LIBTVM_INCLUDE_DIR}
    PRIVATE
        ${LIBTVM_SRC_DIR}
)
target_link_libraries(vm PRIVATE scan)

add_executable(jary ${SRC_DIR}/main.c)
target_include_directories(jary PRIVATE ${SRC_DIR})
target_link_libraries(jary PRIVATE vm)

# CHILD BUILD
add_subdirectory(test)