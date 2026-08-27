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

# RK3572 uses rknn3 backend for both LLM and VLM
if(DEFINED ROCKCHIP_AI_CHIP AND ROCKCHIP_AI_CHIP STREQUAL "rk3572")
    set(RKAI_LLM_WITH_RKNN3 ON CACHE BOOL "" FORCE)
    set(RKAI_VLM_WITH_RKNN3 ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(rkai_LLM)
add_subdirectory(rkai_VLM)


add_executable(rkai_LLM_Demo common/src/rkai_ini.cpp samples/rkai_samples_llm.cpp)

# RK3572 uses rknn3 backend, RV1126B uses rkllm backend
if(DEFINED ROCKCHIP_AI_CHIP AND ROCKCHIP_AI_CHIP STREQUAL "rk3572")
    target_link_libraries(rkai_LLM_Demo
        PRIVATE
        rkai_llm_lib   # from rkai_LLM
    )
else()
    target_link_libraries(rkai_LLM_Demo
        PRIVATE
        rkai_llm_lib   # from rkai_LLM
        ${LIBRKLLMRT}
    )
    # Enable OpenMP for librkllmrt
    target_compile_options(rkai_LLM_Demo PRIVATE -fopenmp)
    target_link_options(rkai_LLM_Demo PRIVATE -fopenmp)
endif()
target_include_directories(rkai_LLM_Demo PUBLIC
     ${CMAKE_CURRENT_SOURCE_DIR}/common/include
)

# VLM demo build rules
if(DEFINED ROCKCHIP_AI_CHIP AND ROCKCHIP_AI_CHIP STREQUAL "rk3572")
    # RK3572 VLM: uses RKNN3 backend (no rkllm, no rknpu2)
    add_executable(rkai_VLM_Demo common/src/cnpy.cpp common/src/rkai_ini.cpp samples/rkai_samples_vlm.cpp)
    target_include_directories(rkai_VLM_Demo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
        ${RKAI_WS_ROOT}/external/rknn3_model_zoo/3rdparty/jpeg_turbo/include
    )
    # Find aarch64 turbojpeg library
    if(NOT DEFINED RKAI_WS_ROOT)
        get_filename_component(RKAI_WS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
    endif()
    find_library(TURBOJPEG_LIB NAMES turbojpeg
        PATHS "${RKAI_WS_ROOT}/external/rknn3_model_zoo/3rdparty/jpeg_turbo/Linux/aarch64"
        NO_DEFAULT_PATH)
    target_link_libraries(rkai_VLM_Demo
        PRIVATE
        rkai_vlm_lib    # from rkai_VLM (built with RKAI_VLM_WITH_RKNN3=ON)
        ${TURBOJPEG_LIB}
    )
else()
    # RV1126B VLM: uses rknpu2 + rkllm
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
    # Enable OpenMP for librkllmrt
    target_compile_options(rkai_VLM_Demo PRIVATE -fopenmp)
    target_link_options(rkai_VLM_Demo PRIVATE -fopenmp)

    if(DEPENDMENT_ROCKIT)
        target_compile_definitions(rkai_VLM_Demo PRIVATE DEPENDMENT_ROCKIT)
        target_link_libraries(rkai_VLM_Demo PRIVATE ${LIBROCKIT} jpeg)
        message(STATUS "rockit detected → DEPENDMENT_ROCKIT enabled")
    else()
        message(STATUS "rockit not detected → DEPENDMENT_ROCKIT disabled")
    endif()
endif()

# Install targets
if(DEFINED ROCKCHIP_AI_CHIP AND ROCKCHIP_AI_CHIP STREQUAL "rk3572")
    install(TARGETS rkai_LLM_Demo rkai_VLM_Demo DESTINATION bin)
else()
    install(TARGETS rkai_LLM_Demo rkai_VLM_Vision_Demo rkai_VLM_Demo DESTINATION bin)
endif()
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/app_conf DESTINATION share)

# Install chip-specific config file for RK3572
if(DEFINED ROCKCHIP_AI_CHIP AND ROCKCHIP_AI_CHIP STREQUAL "rk3572")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/app_conf/config/rkai_cfg_rk3572.ini
            DESTINATION share/app_conf/config
            RENAME rkai_cfg.ini)
endif()

# mkdir a models directory before download model
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/app_conf/models)

if(DEFINED ROCKCHIP_AI_CHIP AND ROCKCHIP_AI_CHIP STREQUAL "rv1126b")
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
