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

