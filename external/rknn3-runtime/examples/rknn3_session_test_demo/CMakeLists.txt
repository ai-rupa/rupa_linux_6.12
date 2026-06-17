cmake_minimum_required(VERSION 3.6)

project(rknn3_session_demo)

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

set(TOKEINZER_PATH ${CMAKE_SOURCE_DIR}/../3rdparty/tokenizer)
set(TOKENIZER_LIB ${TOKEINZER_PATH}/lib/${CMAKE_SYSTEM_NAME}/${LIB_ARCH}/libtokenizer.a)

include_directories(${RKNN3_API_PATH}/include)
include_directories(${TOKEINZER_PATH}/include)
include_directories(${CMAKE_SOURCE_DIR}/../3rdparty/json)

set(CMAKE_INSTALL_RPATH "lib")

# rknn3_session_test
add_executable(rknn3_session_test
  src/rknn3_session_test.cpp
)
target_link_libraries(rknn3_session_test
  ${RKNN3_API_LIB}
  ${TOKENIZER_LIB}
)

# rknn3_session_test_function_call
add_executable(rknn3_session_test_function_call
  src/rknn3_session_test_function_call.cpp
)
target_link_libraries(rknn3_session_test_function_call
  ${RKNN3_API_LIB}
  ${TOKENIZER_LIB}
)

# rknn3_session_test_eval_perf
add_executable(rknn3_session_test_eval_perf
  src/rknn3_session_test_eval_perf.cpp
)
target_link_libraries(rknn3_session_test_eval_perf
  ${RKNN3_API_LIB}
  ${TOKENIZER_LIB}
)

# rknn3_session_test_token_embed_input
add_executable(rknn3_session_test_token_embed_input
  src/rknn3_session_test_token_embed_input.cpp
)
target_link_libraries(rknn3_session_test_token_embed_input
  ${RKNN3_API_LIB}
  ${TOKENIZER_LIB}
)

# install target and libraries
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/rknn3_session_test_${TARGET_SOC}_${CMAKE_SYSTEM_NAME})
install(TARGETS rknn3_session_test DESTINATION ./)
install(TARGETS rknn3_session_test_function_call DESTINATION ./)
install(TARGETS rknn3_session_test_eval_perf DESTINATION ./)
install(TARGETS rknn3_session_test_token_embed_input DESTINATION ./)

install(PROGRAMS ${RKNN3_API_LIB} DESTINATION lib)
