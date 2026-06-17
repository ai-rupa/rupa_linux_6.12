cmake_minimum_required(VERSION 3.15)

file(GLOB RKAI_VLM_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(rkai_vlm_lib STATIC ${RKAI_VLM_SRCS})
target_link_libraries(rkai_vlm_lib
    PUBLIC
    ${OpenCV_LIBS}
)

target_include_directories(rkai_vlm_lib PUBLIC
    ${LIBRKNNRT_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    # ${OPENCV_INCLUDE_DIR}
)

