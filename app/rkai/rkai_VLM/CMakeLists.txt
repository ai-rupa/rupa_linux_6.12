cmake_minimum_required(VERSION 3.15)

file(GLOB RKAI_VLM_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(rkai_vlm_lib STATIC ${RKAI_VLM_SRCS})

# Conditional RKNN3 support for VLM
option(RKAI_VLM_WITH_RKNN3 "Enable rknn3 backend for VLM" OFF)

if (RKAI_VLM_WITH_RKNN3)
    if(NOT DEFINED RKAI_WS_ROOT)
        get_filename_component(RKAI_WS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
    endif()

    # rknn3 api headers and tokenizer
    target_include_directories(rkai_vlm_lib PUBLIC
        "${RKAI_WS_ROOT}/external/rknn3-runtime/rknn3-api/include"
        "${RKAI_WS_ROOT}/external/rknn3-runtime/examples/3rdparty/tokenizer/include"
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    )

    # Link tokenizer and rknn3_api
    find_library(TOKENIZER_LIB NAMES tokenizer
        PATHS "${RKAI_WS_ROOT}/external/rknn3-runtime/examples/3rdparty/tokenizer/lib/Linux/aarch64"
        NO_DEFAULT_PATH)
    if(TOKENIZER_LIB)
        target_link_libraries(rkai_vlm_lib PUBLIC ${TOKENIZER_LIB})
        message(STATUS "VLM: Found tokenizer: ${TOKENIZER_LIB}")
    else()
        message(WARNING "VLM: tokenizer library not found")
    endif()

    find_library(RKNN3_API_LIB NAMES rknn3_api rknn3_api_native
        PATHS "${RKAI_WS_ROOT}/external/rknn3-runtime/rknn3-api/Linux/aarch64"
        NO_DEFAULT_PATH)
    if(RKNN3_API_LIB)
        target_link_libraries(rkai_vlm_lib PUBLIC ${RKNN3_API_LIB} dl)
        message(STATUS "VLM: Found rknn3_api: ${RKNN3_API_LIB}")
    else()
        message(WARNING "VLM: rknn3_api library not found, using -lrknn3_api")
        target_link_libraries(rkai_vlm_lib PUBLIC rknn3_api dl)
    endif()
else()
    # Original rknpu2 path
    target_compile_definitions(rkai_vlm_lib PUBLIC RKAI_DISABLE_RKNN3=1)
    target_include_directories(rkai_vlm_lib PUBLIC
        ${LIBRKNNRT_INCLUDES}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    )
    target_link_libraries(rkai_vlm_lib PUBLIC
        ${OpenCV_LIBS}
    )
endif()
