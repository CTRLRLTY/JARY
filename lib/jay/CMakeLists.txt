target_sources( scanner PRIVATE scanner.c )
target_sources( parser PRIVATE parser.c)
target_sources( dload PRIVATE dload.c )
target_sources( compiler PRIVATE compiler.c defs.c )


if(CMAKE_C_COMPILER_ID MATCHES "^(Clang|GNU)$")
        target_link_options(dload INTERFACE -rdynamic)
endif()

target_include_directories(dload 
        PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
)
target_include_directories(scanner 
        PUBLIC 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/lib/jay
)
target_include_directories(parser 
        PUBLIC 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/lib/jay
)
target_include_directories(compiler 
        PUBLIC 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/lib/jay
)

target_link_libraries( parser PRIVATE scanner )
target_link_libraries( compiler PRIVATE parser dload )

if ( SCRUTINY AND CMAKE_C_COMPILER_ID STREQUAL "GNU" )
        target_compile_options( dload BEFORE PRIVATE -fanalyzer )
        target_compile_options( parser BEFORE PRIVATE -fanalyzer )
        target_compile_options( scanner BEFORE PRIVATE -fanalyzer )
endif()

add_subdirectory( modules ${CMAKE_BINARY_DIR}/modules )
