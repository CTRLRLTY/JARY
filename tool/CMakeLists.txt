add_executable( jassy
	jassy/jassy.c
	${CMAKE_SOURCE_DIR}/lib/jay/compiler.c
	${CMAKE_SOURCE_DIR}/lib/jay/parser.c
	${CMAKE_SOURCE_DIR}/lib/jay/scanner.c
	${CMAKE_SOURCE_DIR}/lib/jay/defs.c
)

target_link_libraries(jassy PRIVATE dload)

target_include_directories( jassy
	PRIVATE
        	${CMAKE_SOURCE_DIR}/lib/jay/
        	${CMAKE_SOURCE_DIR}/include/
)

set_target_properties(jassy PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/)
