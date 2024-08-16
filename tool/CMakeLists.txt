add_executable( astdump
    astdump.c
)

target_link_libraries( astdump
    PRIVATE
        parser
)

target_include_directories( astdump
    PRIVATE
        ${LIBJAYVM_INCLUDE_DIR}
        ${LIBJAYVM_SRC_DIR}
)