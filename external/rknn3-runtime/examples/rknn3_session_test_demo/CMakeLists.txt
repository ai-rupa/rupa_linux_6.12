cmake_minimum_required(VERSION 3.6)

project(rknn3_session_demo)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# skip 3rd-party lib dependencies
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")

# rknn3 api
set(RKNN3_API_PATH ${CMAKE_SOURCE_DIR}/../../rknn3-api)

# Determine architecture based on system and compiler
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(LIB_ARCH x86_64)
  set(RKNN3_API_LIB_EXT ".dll")
elseif(CMAKE_C_COMPILER MATCHES "aarch64")
  set(LIB_ARCH aarch64)
  set(RKNN3_API_LIB_EXT ".so")
else()
  set(LIB_ARCH arm64-v8a)
  set(RKNN3_API_LIB_EXT ".so")
endif()

set(RKNN3_API_LIB ${RKNN3_API_PATH}/${CMAKE_SYSTEM_NAME}/${LIB_ARCH}/librknn3_api${RKNN3_API_LIB_EXT})
set(RKNN3_API_NATIVE_LIB ${RKNN3_API_PATH}/${CMAKE_SYSTEM_NAME}/${LIB_ARCH}/librknn3_api_native${RKNN3_API_LIB_EXT})
set(RKNN3_API_RKCP_LIB ${RKNN3_API_PATH}/${CMAKE_SYSTEM_NAME}/${LIB_ARCH}/librknn3_api_rkcp${RKNN3_API_LIB_EXT})

set(TOKEINZER_PATH ${CMAKE_SOURCE_DIR}/../3rdparty/tokenizer)

# Determine tokenizer library path based on platform
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(TOKENIZER_LIB ${TOKEINZER_PATH}/lib/${CMAKE_SYSTEM_NAME}/${LIB_ARCH}/libtokenizer.a)
elseif(CMAKE_C_COMPILER MATCHES "aarch64")
  set(TOKENIZER_LIB ${TOKEINZER_PATH}/lib/Linux/aarch64/libtokenizer.a)
else()
  set(TOKENIZER_LIB ${TOKEINZER_PATH}/lib/Android/arm64-v8a/libtokenizer.a)
endif()

# Check if tokenizer library exists and enable tokenizer support
set(TOKENIZER_SUPPORT OFF)
if(EXISTS ${TOKENIZER_LIB})
  set(TOKENIZER_SUPPORT ON)
  message(STATUS "Tokenizer library found: ${TOKENIZER_LIB}")
  message(STATUS "Building with tokenizer support enabled")
else()
  message(WARNING "Tokenizer library not found. Building without tokenizer support.")
  set(TOKENIZER_LIB "")
endif()

include_directories(${RKNN3_API_PATH}/include)
include_directories(${TOKEINZER_PATH}/include)
include_directories(${CMAKE_SOURCE_DIR}/../3rdparty/json)

set(CMAKE_INSTALL_RPATH "lib")

# rknn3_session_test
add_executable(rknn3_session_test
  src/rknn3_session_test.cpp
)
if(TOKENIZER_LIB)
  target_link_libraries(rknn3_session_test
    ${RKNN3_API_LIB}
    ${TOKENIZER_LIB}
  )
else()
  target_link_libraries(rknn3_session_test
    ${RKNN3_API_LIB}
  )
endif()

# rknn3_session_test_function_call
add_executable(rknn3_session_test_function_call
  src/rknn3_session_test_function_call.cpp
)
if(TOKENIZER_LIB)
  target_link_libraries(rknn3_session_test_function_call
    ${RKNN3_API_LIB}
    ${TOKENIZER_LIB}
  )
else()
  target_link_libraries(rknn3_session_test_function_call
    ${RKNN3_API_LIB}
  )
endif()

# rknn3_session_test_eval_perf
add_executable(rknn3_session_test_eval_perf
  src/rknn3_session_test_eval_perf.cpp
)
if(TOKENIZER_LIB)
  target_link_libraries(rknn3_session_test_eval_perf
    ${RKNN3_API_LIB}
    ${TOKENIZER_LIB}
  )
else()
  target_link_libraries(rknn3_session_test_eval_perf
    ${RKNN3_API_LIB}
  )
endif()

# rknn3_session_test_token_embed_input
add_executable(rknn3_session_test_token_embed_input
  src/rknn3_session_test_token_embed_input.cpp
)
if(TOKENIZER_LIB)
  target_link_libraries(rknn3_session_test_token_embed_input
    ${RKNN3_API_LIB}
    ${TOKENIZER_LIB}
  )
else()
  target_link_libraries(rknn3_session_test_token_embed_input
    ${RKNN3_API_LIB}
  )
endif()

# install target and libraries
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/rknn3_session_test_${TARGET_SOC}_${CMAKE_SYSTEM_NAME})
install(TARGETS rknn3_session_test DESTINATION ./)
install(TARGETS rknn3_session_test_function_call DESTINATION ./)
install(TARGETS rknn3_session_test_eval_perf DESTINATION ./)
install(TARGETS rknn3_session_test_token_embed_input DESTINATION ./)

install(PROGRAMS ${RKNN3_API_LIB} DESTINATION lib)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  # On Windows, install all DLLs from the rknn3-api Windows directory
  file(GLOB RKNN3_API_DLLS "${RKNN3_API_PATH}/${CMAKE_SYSTEM_NAME}/${LIB_ARCH}/*.dll")
  foreach(dll ${RKNN3_API_DLLS})
    install(PROGRAMS ${dll} DESTINATION lib)
  endforeach()
elseif (TARGET_SOC STREQUAL "RK3572")
  if (EXISTS ${RKNN3_API_NATIVE_LIB})
    install(PROGRAMS ${RKNN3_API_NATIVE_LIB} DESTINATION lib)
  endif()
elseif (TARGET_SOC STREQUAL "RK3576" OR TARGET_SOC STREQUAL "RK3588")
  if (EXISTS ${RKNN3_API_RKCP_LIB})
    install(PROGRAMS ${RKNN3_API_RKCP_LIB} DESTINATION lib)
  endif()
endif()
