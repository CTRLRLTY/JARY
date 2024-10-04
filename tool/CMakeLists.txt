add_executable( jassy )

target_sources( jassy PRIVATE jassy/jassy.c )

target_link_libraries( jassy PRIVATE compiler )

if ( SCRUTINY AND CMAKE_C_COMPILER_ID STREQUAL "GNU" )
        target_compile_options( jassy BEFORE PRIVATE -fanalyzer )
endif()

if ( SANITIZE AND UNIX AND CMAKE_C_COMPILER_ID MATCHES "^(Clang|GNU)$" )
        target_link_options( jassy BEFORE PRIVATE -fsanitize=address -fno-omit-frame-pointer )
endif()

set_target_properties( jassy PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/ )
