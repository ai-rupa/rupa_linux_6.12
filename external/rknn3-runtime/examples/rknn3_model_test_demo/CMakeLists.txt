cmake_minimum_required(VERSION 3.6)

project(rknn3_model_test_demo)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# skip 3rd-party lib dependencies
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")

# rknn3 api
set(RKNN3_API_PATH ${CMAKE_SOURCE_DIR}/../../rknn3-api)

if(CMAKE_C_COMPILER MATCHES "aarch64")
  set(LIB_ARCH aarch64)
else()
  set(LIB_ARCH arm64-v8a)
endif()

set(RKNN3_API_LIB ${RKNN3_API_PATH}/${CMAKE_SYSTEM_NAME}/${LIB_ARCH}/librknn3_api.so)

include_directories(${RKNN3_API_PATH}/include)

set(CMAKE_INSTALL_RPATH "lib")

# rknn3_model_test
add_executable(rknn3_model_test
  src/rknn3_model_test.cpp
  src/cnpy.cpp
)

target_link_libraries(rknn3_model_test
  ${RKNN3_API_LIB}
)

# install target and libraries
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/rknn3_model_test_${CMAKE_SYSTEM_NAME})
install(TARGETS rknn3_model_test DESTINATION ./)

install(PROGRAMS ${RKNN3_API_LIB} DESTINATION lib)
