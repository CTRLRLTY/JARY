set( MODULE_SOURCES 
	mark.c
)

foreach(SOURCE_PATH IN LISTS MODULE_SOURCES)
	cmake_path(GET SOURCE_PATH STEM LIBRARY_NAME)
	add_library(${LIBRARY_NAME} SHARED ${SOURCE_PATH})

	target_include_directories( ${LIBRARY_NAME}
		PRIVATE
			${CMAKE_SOURCE_DIR}/include/
	)

	set_target_properties( ${LIBRARY_NAME} PROPERTIES PREFIX "lib")
endforeach()
