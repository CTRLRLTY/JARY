cmake_minimum_required(VERSION 3.22)

# -- CONFIGURATION
project(jary)

# Enable AddressSanitizer for Debug builds
if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
endif()


# -- DEFINITIONS
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

file(GLOB LIBJAYVM_SRC_DIR ${SRC_DIR}/vm)
file(GLOB LIBJAYVM_INCLUDE_DIR ${LIBJAYVM_SRC_DIR}/include)

# -- TARGETS
add_library(scan 
    ${LIBJAYVM_SRC_DIR}/scanner.c
    ${LIBJAYVM_SRC_DIR}/token.c 
    ${LIBJAYVM_SRC_DIR}/fnv.c
)
target_include_directories(scan
    PUBLIC
        ${LIBJAYVM_INCLUDE_DIR}
    PRIVATE
        ${LIBJAYVM_SRC_DIR}
)

add_library(parser
    ${LIBJAYVM_SRC_DIR}/parser.c
    ${LIBJAYVM_SRC_DIR}/scanner.c
    ${LIBJAYVM_SRC_DIR}/token.c 
    ${LIBJAYVM_SRC_DIR}/fnv.c
    ${LIBJAYVM_SRC_DIR}/ast.c
)
target_include_directories(parser
    PUBLIC
        ${LIBJAYVM_INCLUDE_DIR}
    PRIVATE
        ${LIBJAYVM_SRC_DIR}
)

# add_library(vm ${LIBJAYVM_SRC_DIR}/vm.c)
# target_include_directories(vm 
#     PUBLIC
#         ${LIBJAYVM_INCLUDE_DIR}
#     PRIVATE
#         ${LIBJAYVM_SRC_DIR}
# )
# target_link_libraries(vm PRIVATE scan)

# add_executable(jary ${SRC_DIR}/main.c)
# target_include_directories(jary PRIVATE ${SRC_DIR})
# target_link_libraries(jary PRIVATE vm)

# CHILD BUILD
add_subdirectory(test)
add_subdirectory(tool)