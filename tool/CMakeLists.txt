add_executable( jastdmp
    jastdmp/main.c
)

target_link_libraries( jastdmp
    PRIVATE
        parser
)

if(UNIX)
    target_link_libraries( jastdmp
        PRIVATE
            m # math
    )
endif()

target_include_directories( jastdmp
    PRIVATE
        ${LIBJAYVM_SRC_DIR}
)