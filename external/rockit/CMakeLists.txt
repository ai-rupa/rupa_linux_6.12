cmake_minimum_required(VERSION 3.5)
project (rockit)

if (NOT DEFINED ARCH64)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ARCH64 ON)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(ARCH64 OFF)
    endif()
endif()

if(NOT DEFINED TOOLCHAIN_PRFIX)
  get_filename_component(TOOLCHAIN_PRFIX "${CMAKE_C_COMPILER}" NAME)
  string(REGEX REPLACE "-?(gcc|clang|cc|g\\+\\+|c\\+\\+|clang\\+\\+|gcc\\+\\+)$" "" TOOLCHAIN_PRFIX "${TOOLCHAIN_PRFIX}")
  set(TOOLCHAIN_PRFIX "${TOOLCHAIN_PRFIX}")
endif()

# Chip-specific config: derive CHIP_<NAME> from SOC_NAME
if(DEFINED SOC_NAME)
  string(TOUPPER "${SOC_NAME}" SOC_NAME_UPPER)
  set("CHIP_${SOC_NAME_UPPER}" ON)
endif()

if(RK3308)
set (CHIP_RK3308 ON)
endif()

option(USE_ROCKIT_MPI  "enable rockit mpi" ON)
if (${USE_ROCKIT_MPI})
    message("Build rockit mpi")
    add_subdirectory(mpi)
endif()
