cmake_minimum_required(VERSION 3.15)
project(rockchip_ai_demo)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set (TARGET_LIB_ARCH aarch64)
else()
    set (TARGET_LIB_ARCH armhf)
endif()

if (CMAKE_C_COMPILER MATCHES "uclibc")
        set (TARGET_LIB_ARCH ${TARGET_LIB_ARCH}_uclibc)
endif()

set(LIBRKNNRT librknnrt.so)
set(LIBRKLLMRT librkllmrt.so)
set(LIBROCKIT librockit.so)
add_subdirectory(rkai_LLM)
add_subdirectory(rkai_VLM)


add_executable(rkai_LLM_Demo common/src/rkai_ini.cpp samples/rkai_samples_llm.cpp) 
target_link_libraries(rkai_LLM_Demo
    PRIVATE
    rkai_llm_lib   # from rkai_LLM 
    ${LIBRKLLMRT}
)
target_include_directories(rkai_LLM_Demo PUBLIC
     ${CMAKE_CURRENT_SOURCE_DIR}/common/include
)

add_executable(rkai_VLM_Vision_Demo common/src/cnpy.cpp  samples/rkai_samples_vlm_vision.cpp)
target_link_libraries(rkai_VLM_Vision_Demo
    PRIVATE
    rkai_vlm_lib   # from rkai_VLM
    ${LIBRKNNRT}
    ${OpenCV_LIBS}
)

add_executable(rkai_VLM_Demo common/src/cnpy.cpp common/src/rkai_ini.cpp samples/rkai_samples_vlm.cpp)
target_link_libraries(rkai_VLM_Demo
    PRIVATE
    rkai_vlm_lib    # from rkai_VLM
    ${LIBRKNNRT}
    ${LIBRKLLMRT}
)

if(DEPENDMENT_ROCKIT)
    target_compile_definitions(rkai_VLM_Demo PRIVATE DEPENDMENT_ROCKIT)
    # target_include_directories(rkai_VLM_Demo PRIVATE ${_rockit_inc})
    target_link_libraries(rkai_VLM_Demo PRIVATE ${LIBROCKIT} jpeg)
    message(STATUS "rockit detected → DEPENDMENT_ROCKIT enabled")
else()
    message(STATUS "rockit not detected → DEPENDMENT_ROCKIT disabled")
endif()

install(TARGETS rkai_LLM_Demo rkai_VLM_Vision_Demo rkai_VLM_Demo DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/app_conf DESTINATION share)

# mkdir a models directory before download model
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/app_conf/models)

if(${ROCKCHIP_AI_CHIP} STREQUAL "rv1126b")
    set(MODEL_FILE  "${CMAKE_CURRENT_SOURCE_DIR}/app_conf/models/Qwen3-0.6B_W4A16_RV1126B.rkllm")
    set(MODEL_URL "https://www.modelscope.cn/models/cdd12138/Qwen3-0.6B-w4a16-rkllm/resolve/master/Qwen3-0.6B_W4A16_RV1126B.rkllm")

    # download when models is not exist
    if(NOT EXISTS ${MODEL_FILE})
        find_program(WGET wget)          # find wget
        if(NOT WGET)
            message(FATAL_ERROR "wget not found, please install wget or enable CMake SSL")
        endif()
        execute_process(
            COMMAND ${WGET} -O ${MODEL_FILE} ${MODEL_URL}
            RESULT_VARIABLE RET
            TIMEOUT 300)
        if(NOT RET EQUAL 0)
            file(REMOVE ${MODEL_FILE})   # remove all when fail
            message(FATAL_ERROR "wget download failed: ${RET}")
        endif()
    endif()
endif()
