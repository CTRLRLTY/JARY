add_executable( jassy
	jassy/jassy.c
)

target_link_libraries( jassy
	PRIVATE
		compiler
)

target_include_directories( jassy
	PRIVATE
        	${LIBJAYVM_SRC_DIR}
        	${LIBJAYVM_INCLUDE_DIR}
)