cmake_minimum_required(VERSION 3.8)

project(rkfsmk)

include(FindPkgConfig)

include_directories(
	${PROJECT_SOURCE_DIR}/./
)

link_directories(
	${PROJECT_SOURCE_DIR}/./lib)

link_libraries(
	m
	pthread)

add_definitions(-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1 -DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_UNISTD_H=1 -DHAVE_ERR_H=1 -DHAVE_LINUX_FD_H=1 -DHAVE_LINUX_HDREG_H=1 -DHAVE_LINUX_VERSION_H=1 -DHAVE_SYS_QUEUE_H=1 -DHAVE_ENDIAN_H=1 -DHAVE_DECL_GETMNTENT=1 -DHAVE_DECL_GETMNTINFO=0)

aux_source_directory(./ SRCS)
add_library(${PROJECT_NAME}
	SHARED ${SRCS})

install(TARGETS ${PROJECT_NAME}
	LIBRARY DESTINATION lib)
set(HEADERS
	rkfsmk.h
	)
install(FILES ${HEADERS} DESTINATION include)

add_executable(rkfsmk_tool demo/rkfsmk_tool.c)
target_link_libraries(rkfsmk_tool ${PROJECT_NAME})
install(TARGETS rkfsmk_tool
		RUNTIME DESTINATION bin)

add_executable(repa_mp4_tool demo/repa_mp4_tool.c)
target_link_libraries(repa_mp4_tool ${PROJECT_NAME})
install(TARGETS repa_mp4_tool
                RUNTIME DESTINATION bin)

add_executable(file_write_test demo/file_write_test.c)
target_link_libraries(file_write_test ${PROJECT_NAME})
install(TARGETS file_write_test
		RUNTIME DESTINATION bin)
