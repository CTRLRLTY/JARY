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
