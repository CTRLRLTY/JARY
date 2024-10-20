add_executable( jaryd )

target_sources( jaryd PRIVATE main.c )

target_include_directories( jaryd 
        PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries( jaryd PRIVATE jary )

if ( SCRUTINY AND CMAKE_C_COMPILER_ID STREQUAL "GNU" )
        target_compile_options( jaryd BEFORE PRIVATE -fanalyzer )
endif()

if ( SANITIZE AND UNIX AND CMAKE_C_COMPILER_ID MATCHES "^(Clang|GNU)$" )
        target_link_options( jaryd BEFORE PRIVATE -fsanitize=address -fno-omit-frame-pointer )
endif()

