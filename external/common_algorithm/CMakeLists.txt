#1.cmake version
cmake_minimum_required(VERSION 3.2)

option(COMPILES_EXAMPLES "Enable compiles examples" OFF)
get_filename_component(TOOLCHAIN_PRFIX "${CMAKE_C_COMPILER}" NAME)
string(REGEX REPLACE "-?(gcc|clang|cc|g\\+\\+|c\\+\\+)$" "" TOOLCHAIN_PRFIX "${TOOLCHAIN_PRFIX}")
set(TOOLCHAIN_PRFIX "${TOOLCHAIN_PRFIX}")
add_subdirectory(audio)
add_subdirectory(video)
add_subdirectory(misc)
