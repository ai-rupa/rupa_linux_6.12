#1.cmake version
cmake_minimum_required(VERSION 3.2)

#2.project name
project(RKAUDIOALGORITHMS)

# ----------------------------------------------------------------------------
#  install resource
# ----------------------------------------------------------------------------
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/conf/
	DESTINATION "vqefiles"
	FILES_MATCHING PATTERN "*.json"
)

# ----------------------------------------------------------------------------
#  install headers
# ----------------------------------------------------------------------------
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
	DESTINATION "include"
	FILES_MATCHING PATTERN "*.h"
)

# ----------------------------------------------------------------------------
#  install libs
# ----------------------------------------------------------------------------
if (RV1103B)
set(uclibc_src uclibc_rv1103b)
else()
set(uclibc_src uclibc)
endif()
if (USE_UCLIBC)
	if(RKAPPLUS_AED_ENABLE OR RKAPPLUS_ENABLE)
		install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${uclibc_src}/librkaudio_common.so
				${CMAKE_CURRENT_SOURCE_DIR}/${uclibc_src}/librkaudio_common.a
			DESTINATION "lib"
		)
		install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/conf/
			DESTINATION "vqefiles"
			FILES_MATCHING PATTERN "*.rknn"
		)
		if (RV1103B)
			install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/conf_rv1103b/
				DESTINATION "vqefiles"
				FILES_MATCHING PATTERN "*.*"
			)
		endif()
		if (RV1106_RV1103)
			install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/conf_rv1106_rv1103/
				DESTINATION "vqefiles"
				FILES_MATCHING PATTERN "*.*"
			)
		endif()
	endif()

	if(RKAPPLUS_ENABLE)
		install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${uclibc_src}/librkaudio_vqe.so
			${CMAKE_CURRENT_SOURCE_DIR}/${uclibc_src}/librkaudio_vqe.a
			DESTINATION "lib"
		)
	endif()

	if (RKAPPLUS_AED_ENABLE)
		install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${uclibc_src}/librkaudio_detect.so
				${CMAKE_CURRENT_SOURCE_DIR}/${uclibc_src}/librkaudio_detect.a
			DESTINATION "lib"
		)
	endif()

	if (RKAPPLUS_EFFECT_EQDRC_ENABLE)
		install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${uclibc_src}/librkaudio_effect_eqdrc.so
			      ${CMAKE_CURRENT_SOURCE_DIR}/${uclibc_src}/librkaudio_effect_eqdrc.a
			DESTINATION "lib"
		)
		install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/conf/
			DESTINATION "vqefiles"
			FILES_MATCHING PATTERN "*.bin"
		)
	endif()
endif()

if (USE_32BIT)
	if(RKAPPLUS_AED_ENABLE OR RKAPPLUS_ENABLE)
		if (TOOLCHAIN_PRFIX MATCHES "-musl")
			install(
				FILES ${CMAKE_CURRENT_SOURCE_DIR}/${TOOLCHAIN_PRFIX}/librkaudio_common.so
				${CMAKE_CURRENT_SOURCE_DIR}/${TOOLCHAIN_PRFIX}/librkaudio_common.a
				DESTINATION "lib"
			)
		else()
		install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib32/librkaudio_common.so
				${CMAKE_CURRENT_SOURCE_DIR}/lib32/librkaudio_common.a
			DESTINATION "lib"
		)
		endif()

		if (RV1126B)
			install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/conf_rv1126b/
				DESTINATION "vqefiles"
				FILES_MATCHING PATTERN "*.*"
			)
		endif()
	endif()

	if(RKAPPLUS_ENABLE)
		if (TOOLCHAIN_PRFIX MATCHES "-musl")
			install(
				FILES ${CMAKE_CURRENT_SOURCE_DIR}/${TOOLCHAIN_PRFIX}/librkaudio_vqe.so
				${CMAKE_CURRENT_SOURCE_DIR}/${TOOLCHAIN_PRFIX}/librkaudio_vqe.a
				DESTINATION "lib"
			)
		else()
		install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib32/librkaudio_vqe.so
			${CMAKE_CURRENT_SOURCE_DIR}/lib32/librkaudio_vqe.a
			DESTINATION "lib"
		)
		endif()
	endif()

	if (RKAPPLUS_AED_ENABLE)
		if (TOOLCHAIN_PRFIX MATCHES "-musl")
			install(
				FILES ${CMAKE_CURRENT_SOURCE_DIR}/${TOOLCHAIN_PRFIX}/librkaudio_detect.so
				${CMAKE_CURRENT_SOURCE_DIR}/${TOOLCHAIN_PRFIX}/librkaudio_detect.a
				DESTINATION "lib"
			)
		else()
		install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib32/librkaudio_detect.so
				${CMAKE_CURRENT_SOURCE_DIR}/lib32/librkaudio_detect.a
			DESTINATION "lib"
	)
		endif()
	endif()

	if (RKAPPLUS_EFFECT_EQDRC_ENABLE)
		if (TOOLCHAIN_PRFIX MATCHES "-musl")
			install(
				FILES ${CMAKE_CURRENT_SOURCE_DIR}/${TOOLCHAIN_PRFIX}/librkaudio_effect_eqdrc.so
				${CMAKE_CURRENT_SOURCE_DIR}/${TOOLCHAIN_PRFIX}/librkaudio_effect_eqdrc.a
				DESTINATION "lib"
			)
		else()
		install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib32/librkaudio_effect_eqdrc.so
			      ${CMAKE_CURRENT_SOURCE_DIR}/lib32/librkaudio_effect_eqdrc.a
			DESTINATION "lib"
		)
		endif()
		install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/conf/
			DESTINATION "vqefiles"
			FILES_MATCHING PATTERN "*.bin"
		)
	endif()

	if (RV1126B)
		if (RKAPPLUS_AINR_ENABLE)
			install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib32/librkaudio_vqe_ainr_rv1126b.so
				DESTINATION "lib"
				RENAME librkaudio_vqe.so
			)
			install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib32/librkaudio_vqe_ainr_rv1126b.a
				DESTINATION "lib"
				RENAME librkaudio_vqe.a
			)
			install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib32/librkaudio_common_ainr_rv1126b.so
				DESTINATION "lib"
				RENAME librkaudio_common.so
			)
			install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib32/librkaudio_common_ainr_rv1126b.a
				DESTINATION "lib"
				RENAME librkaudio_common.a
			)
			install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/conf_ainr/config_aivqe_ainr.json
				DESTINATION "vqefiles"
				RENAME config_aivqe.json
			)
		endif()
	endif()
endif()

if (USE_64BIT)
	if(RKAPPLUS_AED_ENABLE OR RKAPPLUS_ENABLE)
		install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib64/librkaudio_common.so
				${CMAKE_CURRENT_SOURCE_DIR}/lib64/librkaudio_common.a
			DESTINATION "lib"
		)

		if (RV1126B)
			install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/conf_rv1126b/
				DESTINATION "vqefiles"
				FILES_MATCHING PATTERN "*.*"
			)
		endif()
	endif()

	if(RKAPPLUS_ENABLE)
		install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib64/librkaudio_vqe.so
			${CMAKE_CURRENT_SOURCE_DIR}/lib64/librkaudio_vqe.a
			DESTINATION "lib"
		)
	endif()

	if (RKAPPLUS_AED_ENABLE)
		install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib64/librkaudio_detect.so
				${CMAKE_CURRENT_SOURCE_DIR}/lib64/librkaudio_detect.a
			DESTINATION "lib"
	)
	endif()

	if (RKAPPLUS_EFFECT_EQDRC_ENABLE)
		install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib64/librkaudio_effect_eqdrc.so
			      ${CMAKE_CURRENT_SOURCE_DIR}/lib64/librkaudio_effect_eqdrc.a
			DESTINATION "lib"
		)
		install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/conf/
			DESTINATION "vqefiles"
			FILES_MATCHING PATTERN "*.bin"
		)
	endif()

	if (RV1126B)
		if (RKAPPLUS_AINR_ENABLE)
			install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib64/librkaudio_vqe_ainr_rv1126b.so
				DESTINATION "lib"
				RENAME librkaudio_vqe.so
			)
			install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib64/librkaudio_vqe_ainr_rv1126b.a
				DESTINATION "lib"
				RENAME librkaudio_vqe.a
			)
			install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib64/librkaudio_common_ainr_rv1126b.so
				DESTINATION "lib"
				RENAME librkaudio_common.so
			)
			install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/lib64/librkaudio_common_ainr_rv1126b.a
				DESTINATION "lib"
				RENAME librkaudio_common.a
			)
			install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/conf_ainr/config_aivqe_ainr.json
				DESTINATION "vqefiles"
				RENAME config_aivqe.json
			)
		endif()
	endif()
endif()
