add_executable( jassy
    jassy/jassy.c
)

target_link_libraries( jassy
    PRIVATE
        parser
)

if(UNIX)
    target_link_libraries( jassy
        PRIVATE
            m # math
    )
endif()

target_include_directories( jassy
    PRIVATE
        ${LIBJAYVM_SRC_DIR}
)