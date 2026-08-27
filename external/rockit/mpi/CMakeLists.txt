cmake_minimum_required(VERSION 3.5)
project (rockit)
include_directories(sdk/include)
if(ARCH64)
    include_directories(sdk/lib/lib64)
    install(FILES sdk/lib/lib64/rk_defines.h DESTINATION "include")
    set(ROCKIT_DEP_COMMON_LIBS
        ${CMAKE_CURRENT_LIST_DIR}/sdk/lib/lib64/librockit.so
    )
    set(ROCKIT_FILE_LIBS
        ${ROCKIT_DEP_COMMON_LIBS}
        ${CMAKE_CURRENT_LIST_DIR}/sdk/lib/lib64/libgraphic_lsf.so
        ${CMAKE_CURRENT_LIST_DIR}/sdk/lib/lib64/libRkSwCac.so
        ${CMAKE_CURRENT_LIST_DIR}/sdk/lib/lib64/libturbojpeg.so
        ${CMAKE_CURRENT_LIST_DIR}/sdk/lib/lib64/libjpeg.so 
    )
    set(ROCKIT_DUMPSYS_FILE ${CMAKE_CURRENT_LIST_DIR}/example/bin/bin64/dumpsys)
else()
    if(CHIP_RK3308)
        include_directories(sdk/lib/lib32/rk3308)
        install(FILES sdk/lib/lib32/rk3308/rk_defines.h DESTINATION "include")
        set(ROCKIT_DEP_COMMON_LIBS
            ${CMAKE_CURRENT_LIST_DIR}/sdk/lib/lib32/rk3308/librockit.so
        )
        set(ROCKIT_FILE_LIBS
            ${ROCKIT_DEP_COMMON_LIBS}
        )
    elseif(DEFINED SOC_NAME)
        set(CHIP_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/sdk/lib/lib32/${SOC_NAME}")
        set(CHIP_VER_DIR "${CHIP_LIB_DIR}/${TOOLCHAIN_PRFIX}")
        include_directories("${CHIP_LIB_DIR}")
        include_directories("${CHIP_VER_DIR}")
        install(FILES "${CHIP_LIB_DIR}/rk_defines.h" DESTINATION "include")
        set(ROCKIT_DEP_COMMON_LIBS "${CHIP_VER_DIR}/librockit.so")
        set(ROCKIT_FILE_LIBS
            ${ROCKIT_DEP_COMMON_LIBS}
            ${CHIP_VER_DIR}/librockit.a
        )
    else()
        include_directories(sdk/lib/lib32)
        install(FILES sdk/lib/lib32/rk_defines.h DESTINATION "include")
        set(ROCKIT_DEP_COMMON_LIBS
            ${CMAKE_CURRENT_LIST_DIR}/sdk/lib/lib32/librockit.so
        )
        set(ROCKIT_FILE_LIBS
            ${ROCKIT_DEP_COMMON_LIBS}
            ${CMAKE_CURRENT_LIST_DIR}/sdk/lib/lib32/libgraphic_lsf.so
        )
    endif()
    set(ROCKIT_DUMPSYS_FILE ${CMAKE_CURRENT_LIST_DIR}/example/bin/bin32/dumpsys)
endif()

add_subdirectory(sdk)
if(CHIP_RK3308)
else()
add_subdirectory(example)
endif()
