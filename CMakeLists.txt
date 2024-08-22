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
add_library(scan 
    ${LIBJAYVM_SRC_DIR}/scanner.c
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

# add_subdirectory(test)
add_subdirectory(tool)