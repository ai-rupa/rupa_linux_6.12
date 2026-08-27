cmake_minimum_required(VERSION 3.15)

# source file
file(GLOB RKAi_LLM_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(rkai_llm_lib STATIC ${RKAi_LLM_SRCS})

target_include_directories(rkai_llm_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
     ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# Disable RKNN3 backend by default due to API conflicts with rkllm.h
option(RKAI_LLM_WITH_RKNN3 "Enable rknn3 session backend" OFF)

if (RKAI_LLM_WITH_RKNN3)
    # Use RKAI_WS_ROOT if set, otherwise compute from current directory
    if(NOT DEFINED RKAI_WS_ROOT)
        get_filename_component(RKAI_WS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
    endif()

    # rknn3 api headers and tokenizer
    target_include_directories(rkai_llm_lib PUBLIC
        "${RKAI_WS_ROOT}/external/rknn3-runtime/rknn3-api/include"
        "${RKAI_WS_ROOT}/external/rknn3-runtime/examples/3rdparty/tokenizer/include"
    )

    # link prebuilt tokenizer static library
    find_library(TOKENIZER_LIB NAMES tokenizer
        PATHS "${RKAI_WS_ROOT}/external/rknn3-runtime/examples/3rdparty/tokenizer/lib/Linux/aarch64"
        NO_DEFAULT_PATH)
    if(TOKENIZER_LIB)
        target_link_libraries(rkai_llm_lib PUBLIC ${TOKENIZER_LIB})
        message(STATUS "Found tokenizer: ${TOKENIZER_LIB}")
    else()
        message(WARNING "tokenizer library not found")
    endif()

    # Link rknn3 runtime library
    find_library(RKNN3_API_LIB NAMES rknn3_api rknn3_api_native
        PATHS "${RKAI_WS_ROOT}/external/rknn3-runtime/rknn3-api/Linux/aarch64"
        NO_DEFAULT_PATH)
    if(RKNN3_API_LIB)
        target_link_libraries(rkai_llm_lib PUBLIC ${RKNN3_API_LIB} dl)
        message(STATUS "Found rknn3_api: ${RKNN3_API_LIB}")
    else()
        message(WARNING "rknn3_api library not found, using -lrknn3_api")
        target_link_libraries(rkai_llm_lib PUBLIC rknn3_api dl)
    endif()
else()
    target_compile_definitions(rkai_llm_lib PUBLIC RKAI_DISABLE_RKNN3=1)
endif()

