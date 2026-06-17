cmake_minimum_required(VERSION 2.8.0 FATAL_ERROR)
option(COMPILE_FOR_UVC_TINY_RV1126B_4K "compile for uvc tiny rv1126b 4k" OFF)
option(COMPILE_FOR_UVC_TINY_RV1126B_WINHELLO "compile for uvc tiny rv1126b winhello" OFF)
option(COMPILE_FOR_UVC_TINY_RK3588_4K "compile for uvc tiny rk3588 4k" OFF)
set(CMAKE_CXX_STANDARD 11)
add_definitions(-DDBUG)
add_definitions(-g)
PROJECT(rk_mpi_uvc)
include(FindPkgConfig)

add_definitions(
		-Wall -g -O3 -ggdb -gdwarf -funwind-tables -rdynamic
		-Wno-unused-function
		-Wno-unused-parameter -D_GNU_SOURCE
		)

if (COMPILE_FOR_UVC_TINY_RV1126B_4K)
    message(STATUS "Build Mode: RV1126B 4K")
    set(RK_UVC_ENABLE_ASD ON)
    set(RK_UVC_ENABLE_EPTZ ON)
    set(RK_UVC_ENABLE_IVA ON)
    set(RK_UVC_ENABLE_NN ON)
    set(RK_UVC_ENABLE_FACE_NN OFF)
    set(RK_UVC_ENABLE_FACE_POSE OFF)
    set(UVC_INI_SRC configs/rkuvc.ini)
    set(UVC_CFG_SRC configs/uvc_mpi_cfg_rv1126b_4k.conf)
    set(USB_CFG_SRC configs/usb_config_rv1126b_4k.sh)

elseif(COMPILE_FOR_UVC_TINY_RV1126B_WINHELLO)
    message(STATUS "Build Mode: RV1126B WinHello")
    set(RK_UVC_ENABLE_ASD ON)
    set(RK_UVC_ENABLE_EPTZ ON)
    set(RK_UVC_ENABLE_NN ON)
    set(RK_UVC_ENABLE_IVA ON)
    set(RK_UVC_ENABLE_FACE_NN OFF)
    set(RK_UVC_ENABLE_FACE_POSE OFF)
    set(UVC_INI_SRC configs/rkuvc_winhello.ini)
    set(UVC_CFG_SRC configs/uvc_mpi_cfg_rv1126b_winhello.conf)
    set(USB_CFG_SRC configs/usb_config_rv1126b_winhello.sh)

elseif (COMPILE_FOR_UVC_TINY_RK3588_4K)
    message(STATUS "Build Mode: RK3588 4K")
    set(RK_UVC_ENABLE_ASD OFF)
    set(RK_UVC_ENABLE_EPTZ ON)
    set(RK_UVC_ENABLE_NN ON)
    set(RK_UVC_ENABLE_IVA OFF)
    set(RK_UVC_ENABLE_FACE_NN ON)
    set(UVC_INI_SRC configs/rkuvc.ini)
    set(UVC_CFG_SRC configs/uvc_mpi_cfg_rk3588_4k.conf)
    set(USB_CFG_SRC configs/usb_config_rk3588_4k.sh)

else()
    message(STATUS "Build Mode: Default")
    set(RK_UVC_ENABLE_ASD OFF)
    set(RK_UVC_ENABLE_EPTZ OFF)
    set(RK_UVC_ENABLE_NN OFF)
    set(RK_UVC_ENABLE_IVA OFF)
    set(RK_UVC_ENABLE_FACE_NN OFF)
    set(UVC_INI_SRC configs/rkuvc.ini)
    set(UVC_CFG_SRC configs/uvc_mpi_cfg.conf)
    set(USB_CFG_SRC configs/usb_config.sh)
endif()

if (RK_UVC_ENABLE_FASTBOOT)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-multiple-definition")
endif()

if (RK_UVC_ENABLE_EPTZ)
    add_definitions(-DUVC_ENABLE_EPTZ)
    message(STATUS "uvc enable eptz")
endif()

if (RK_UVC_ENABLE_IVA)
    add_definitions(-DUVC_ENABLE_IVA)
    message(STATUS "uvc enable iva")
endif()

if (RK_UVC_ENABLE_FACE_NN)
    add_definitions(-DUVC_ENABLE_FACE_NN)
    message(STATUS "uvc enable face nn")
endif()

if (RK_UVC_ENABLE_FACE_POSE)
    add_definitions(-DUVC_ENABLE_FACE_POSE)
    message(STATUS "uvc enable face pose")
endif()

if (RK_UVC_ENABLE_NN)
    add_definitions(-DUVC_ENABLE_NN)
    message(STATUS "uvc enable nn")
endif()

if (RK_UVC_ENABLE_ASD)
    add_definitions(-DUVC_ENABLE_ASD)
    message(STATUS "uvc enable asd")
endif()

include_directories(uvc)
include_directories(uvc/cJSON)
include_directories(isp)
include_directories(param)
include_directories(eptz)
include_directories(face)

include_directories(${PROJECT_SOURCE_DIR}/include
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/uAPI
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/uAPI2
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/algos
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/common
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/xcore
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/iq_parser
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/iq_parser_v2
                                        ${CMAKE_SYSROOT}/usr/include/libdrm
                                        )

option(COMPILE_FOR_UVC_UAC "compile for uac" OFF)

# If this mode is enabled, mono audio output will be used.
# You must also modify the following functions in the usb_config.sh configuration file:
# uac1_device_config and uac2_device_config
# The following parameters must be set to 1:
# p_chmask (playback channel mask)
# c_chmask (recording channel mask)
option(RK_UAC_USE_MONO "UAC Mono Mode" OFF)

if (RK_UAC_USE_MONO)
    option(RK_UAC_OPEN_VQE "UAC OPEN VQE" OFF)
endif()

if(COMPILE_FOR_UVC_UAC)
  if(RK_UAC_USE_MONO)
    add_definitions(-DUAC_MONO_MODE)
    if(RK_UAC_OPEN_VQE)
      add_definitions(-DUAC_OPEN_VQE)
    endif()
   endif()
  include(uac/uac.cmake)
endif()

if(${RK_APP_CHIP} STREQUAL "rv1106")
    include_directories(isp/rv1106)
    set(ISP_SOURCE isp/rv1106/isp.c)
    set(UVC_LIBRARY_DIR ${PROJECT_SOURCE_DIR}/lib/rv1106)
    add_definitions(-DUVC_IO_METHOD_USERPTR)
    add_definitions(-DRV1106)

elseif(${RK_APP_CHIP} STREQUAL "rv1103b")
    include_directories(isp/rv1103b)
    set(ISP_SOURCE isp/rv1103b/isp.c)
    set(UVC_LIBRARY_DIR ${PROJECT_SOURCE_DIR}/lib/rv1103b)
    add_definitions(-DRV1103B)
    add_definitions(-DUVC_IO_METHOD_USERPTR)

elseif(${RK_APP_CHIP} STREQUAL "rv1126b")
    include_directories(isp/rv1126b)
    set(UVC_LIBRARY_DIR ${PROJECT_SOURCE_DIR}/lib/rv1126b)
    set(ISP_SOURCE isp/rv1126b/isp.c)
    add_definitions(-DRV1126B)
    add_definitions(-DUVC_IO_METHOD_MMAP)
    set(UVC_FACE_MODEL "rv1126b_face_detection_v1.data")

elseif(${RK_APP_CHIP} STREQUAL "rk3588")
    include_directories(isp/rk3588)
    set(UVC_LIBRARY_DIR ${PROJECT_SOURCE_DIR}/lib/rk3588)
    set(ISP_SOURCE isp/rk3588/isp.c)
    add_definitions(-DRK3588)
    add_definitions(-DUVC_IO_METHOD_MMAP)
    set(UVC_FACE_MODEL "rk3588_face_detection_v2.data")

elseif(${RK_APP_CHIP} STREQUAL "rk3572")
    include_directories(isp/rk3572)
    set(UVC_LIBRARY_DIR ${PROJECT_SOURCE_DIR}/lib/rk3572)
    set(ISP_SOURCE isp/rk3572/isp.c)
    add_definitions(-DRK3572)
    add_definitions(-DUVC_IO_METHOD_MMAP)

else()
message(FATAL_ERROR "\n\n!!! FATAL ERROR !!!\n"
                    "Unsupported chip type: ${RK_APP_CHIP}\n"
                    "Supported chip types are: rv1106, rv1103b, rv1126b, rk3588, rk3572\n"
                    "Please specify a valid chip type using -DRK_APP_CHIP=<chip>\n")
endif()

set(LIB_SOURCE
    uvc/uvc_process.cpp
    uvc/uvc-gadget.c
    uvc/uvc_video.cpp
    uvc/uvc_control.cpp
    uvc/uvc_process.cpp
    uvc/uvc_mpi_config.c
    uvc/uvc_mpi_venc.cpp
    uvc/uvc_mpi_vi.cpp
    uvc/uvc_mpi_vpss.cpp
    uvc/uevent.c
    uvc/uvc_configfs.c
    uvc/camera_control.c
    uvc/camera_pu_control.c
    uvc/cJSON/cJSON.c
    uvc/osd.c
    param/dictionary.c
    param/iniparser.c
    param/param.c
)

if (RK_UVC_ENABLE_NN)
    list(APPEND LIB_SOURCE uvc/nn_process.cpp face/face_detector.cpp)
endif()

if (RK_UVC_ENABLE_IVA)
    list(APPEND LIB_SOURCE face/rockiva_detector.cpp)
    list(APPEND LIB_SOURCE face/face_pose.cpp)
endif()

if (RK_UVC_ENABLE_EPTZ)
    list(APPEND LIB_SOURCE eptz/eptz.c)
endif()

if (RK_UVC_ENABLE_FACE_NN)
    list(APPEND LIB_SOURCE face/face_nn_detector.cpp)
    list(APPEND LIB_SOURCE face/face_pose.cpp)
endif()

if(COMPILE_FOR_UVC_UAC)
    add_definitions(-DCOMPILE_FOR_UVC_UAC)
    set(SOURCE
        main.c
        ${ISP_SOURCE}
        ${LIB_SOURCE}
        ${UAC_SOURCE}
    )
else()
    set(SOURCE
        main.c
        ${ISP_SOURCE}
        ${LIB_SOURCE}
    )
endif()

if (RK_UVC_ENABLE_FASTBOOT)
    set(RK_MPI_UVC_DEPENDENT_LIBS
        pthread
        rt
        rkaiq
        librockit.a
        librockchip_mpp.a
        librkaudio_detect.a
        librga.a
    )
    if (RK_UVC_ENABLE_IVA)
        list(APPEND RK_MPI_UVC_DEPENDENT_LIBS librockiva.so)
    endif()
    if (RK_UVC_ENABLE_EPTZ)
        list(APPEND RK_MPI_UVC_DEPENDENT_LIBS libeptz.so)
    endif()
    if (RK_UVC_ENABLE_FACE_NN OR RK_UVC_ENABLE_FACE_POSE)
        list(APPEND RK_MPI_UVC_DEPENDENT_LIBS libfaceDet.so)
    endif()
    message(STATUS "RK_UVC_ENABLE_FASTBOOT is enabled.")
else()
    set(RK_MPI_UVC_DEPENDENT_LIBS
        pthread
        rt
        rkaiq
        rockit
    )
    if (RK_UVC_ENABLE_IVA)
        list(APPEND RK_MPI_UVC_DEPENDENT_LIBS rockiva)
    endif()
    if (RK_UVC_ENABLE_EPTZ)
        list(APPEND RK_MPI_UVC_DEPENDENT_LIBS eptz)
    endif()
    if (RK_UVC_ENABLE_FACE_NN OR RK_UVC_ENABLE_FACE_POSE)
        list(APPEND RK_MPI_UVC_DEPENDENT_LIBS faceDet)
    endif()
    message(STATUS "RK_UVC_ENABLE_FASTBOOT is disabled.")
endif()

link_directories(${UVC_LIBRARY_DIR})

ADD_EXECUTABLE(rk_mpi_uvc ${SOURCE})

target_link_libraries(rk_mpi_uvc ${RK_MPI_UVC_DEPENDENT_LIBS})

if (RK_UAC_OPEN_VQE)
    if (RK_UVC_ENABLE_FASTBOOT)
        target_link_libraries(rk_mpi_uvc PRIVATE
            "-Wl,--whole-archive"
            rkaudio_vqe.a
            "-Wl,--no-whole-archive"
            rkaudio_common.a
        )
    else()
        target_link_libraries(rk_mpi_uvc
            rkaudio_vqe
            rkaudio_common
        )
    endif()
endif()

set(UVC_CFG_DST uvc_mpi_cfg.conf)
set(UVC_INI_DST rkuvc.ini)
set(USB_CFG_DST usb_config.sh)
install(DIRECTORY ./uvc DESTINATION include
        FILES_MATCHING PATTERN "*.h")
install(TARGETS rk_mpi_uvc DESTINATION bin)
install(FILES ${UVC_INI_SRC} DESTINATION share RENAME ${UVC_INI_DST})
install(FILES ${UVC_CFG_SRC} DESTINATION share RENAME ${UVC_CFG_DST})
install(FILES ${USB_CFG_SRC} DESTINATION bin RENAME ${USB_CFG_DST}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_WRITE GROUP_EXECUTE
        WORLD_READ WORLD_WRITE WORLD_EXECUTE)
if (COMPILE_FOR_BUILDROOT)
add_definitions(-DUSE_BUILDROOT)
install(FILES RkLunch-buildroot.sh DESTINATION bin RENAME RkLunch.sh
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_WRITE GROUP_EXECUTE
        WORLD_READ WORLD_WRITE WORLD_EXECUTE)
else()
install(FILES RkLunch.sh DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_WRITE GROUP_EXECUTE
        WORLD_READ WORLD_WRITE WORLD_EXECUTE)
endif()
install(FILES RkLunch-stop.sh DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_WRITE GROUP_EXECUTE
        WORLD_READ WORLD_WRITE WORLD_EXECUTE)

if (RK_UVC_ENABLE_EPTZ)
    install(FILES ${UVC_LIBRARY_DIR}/libeptz.so DESTINATION "lib")
endif()

if (RK_UVC_ENABLE_FACE_NN OR RK_UVC_ENABLE_FACE_POSE)
    install(FILES ${UVC_LIBRARY_DIR}/libfaceDet.so DESTINATION "lib")
    install(FILES ${UVC_LIBRARY_DIR}/${UVC_FACE_MODEL} DESTINATION "lib")
endif()
