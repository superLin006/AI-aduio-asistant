cmake_minimum_required(VERSION 3.16)

option(AIAUDIO_BUILD_DEMOS "Build command-line demos" ON)
option(AIAUDIO_BUILD_TESTS "Build unit tests" ON)
# Cross-build mount paths such as /llm-sdk must never leak into board binaries.
set(CMAKE_SKIP_BUILD_RPATH TRUE)
set(SHERPA_ONNX_INSTALL_DIR "" CACHE PATH "Installed sherpa-onnx SDK root")
set(SHERPA_ONNX_INCLUDE_ROOT "" CACHE PATH "Optional sherpa-onnx source/include root")
set(SHERPA_ONNX_LIBRARY_ROOT "" CACHE PATH "Optional sherpa-onnx library root")
set(SOPHON_SDK_DIR "" CACHE PATH "Sophon SDK containing include/ and lib/")
set(LLM_SDK_DIR "" CACHE PATH "Dispatch LLM SDK root containing include/ and lib/")
set(ONNXRUNTIME_ROOT "" CACHE PATH "Unified ONNX Runtime 1.24.4 root")

if(NOT SHERPA_ONNX_INSTALL_DIR)
  message(FATAL_ERROR "Set SHERPA_ONNX_INSTALL_DIR to a sherpa-onnx install containing include/ and lib/")
endif()

if(NOT SHERPA_ONNX_INCLUDE_ROOT)
  set(SHERPA_ONNX_INCLUDE_ROOT "${SHERPA_ONNX_INSTALL_DIR}/include")
endif()
if(NOT SHERPA_ONNX_LIBRARY_ROOT)
  set(SHERPA_ONNX_LIBRARY_ROOT "${SHERPA_ONNX_INSTALL_DIR}/lib")
endif()

find_path(SHERPA_ONNX_INCLUDE_DIR sherpa-onnx/c-api/cxx-api.h
  PATHS "${SHERPA_ONNX_INCLUDE_ROOT}" NO_DEFAULT_PATH REQUIRED)
find_library(SHERPA_ONNX_CXX_LIBRARY sherpa-onnx-cxx-api
  PATHS "${SHERPA_ONNX_LIBRARY_ROOT}" NO_DEFAULT_PATH REQUIRED)
find_library(SHERPA_ONNX_C_LIBRARY sherpa-onnx-c-api
  PATHS "${SHERPA_ONNX_LIBRARY_ROOT}" NO_DEFAULT_PATH REQUIRED)
if(NOT ONNXRUNTIME_ROOT)
  message(FATAL_ERROR "Set ONNXRUNTIME_ROOT to the shared ONNX Runtime 1.24.4 SDK")
endif()
find_library(ONNXRUNTIME_LIBRARY onnxruntime
  PATHS "${ONNXRUNTIME_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)
find_library(BMRT_LIBRARY bmrt
  PATHS "${SOPHON_SDK_DIR}/lib" NO_DEFAULT_PATH REQUIRED)
find_library(BMLIB_LIBRARY bmlib
  PATHS "${SOPHON_SDK_DIR}/lib" NO_DEFAULT_PATH REQUIRED)
find_path(LLM_SDK_INCLUDE_DIR IntentService.h
  PATHS "${LLM_SDK_DIR}/include" NO_DEFAULT_PATH REQUIRED)
find_library(LLM_SDK_LIBRARY llmsdk
  PATHS "${LLM_SDK_DIR}/lib" NO_DEFAULT_PATH REQUIRED)

add_library(ai_audio_assistant SHARED
  src/assistant_pipeline.cpp src/intent_recognizer.cpp src/offline_asr.cpp)
add_library(ai_audio_assistant::ai_audio_assistant ALIAS ai_audio_assistant)
target_compile_features(ai_audio_assistant PUBLIC cxx_std_17)
target_include_directories(ai_audio_assistant
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PRIVATE "${SHERPA_ONNX_INCLUDE_DIR}" "${LLM_SDK_INCLUDE_DIR}")
target_link_libraries(ai_audio_assistant PUBLIC
  "${SHERPA_ONNX_CXX_LIBRARY}" "${SHERPA_ONNX_C_LIBRARY}"
  "${LLM_SDK_LIBRARY}" "${ONNXRUNTIME_LIBRARY}"
  "${BMRT_LIBRARY}" "${BMLIB_LIBRARY}")
set_target_properties(ai_audio_assistant PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES)

if(AIAUDIO_BUILD_DEMOS)
  add_executable(ai_audio_offline_demo apps/offline_demo.cpp)
  target_link_libraries(ai_audio_offline_demo PRIVATE ai_audio_assistant)

  add_executable(ai_audio_pipeline_demo apps/pipeline_demo.cpp)
  target_include_directories(ai_audio_pipeline_demo PRIVATE "${SHERPA_ONNX_INCLUDE_DIR}")
  target_link_libraries(ai_audio_pipeline_demo PRIVATE ai_audio_assistant)

  add_executable(ai_audio_intent_demo apps/intent_demo.cpp)
  target_link_libraries(ai_audio_intent_demo PRIVATE ai_audio_assistant)

  add_executable(ai_audio_unified_demo apps/unified_demo.cpp)
  target_link_libraries(ai_audio_unified_demo PRIVATE ai_audio_assistant)

  add_executable(ai_audio_voice_intent_demo apps/voice_intent_demo.cpp)
  target_include_directories(ai_audio_voice_intent_demo PRIVATE "${SHERPA_ONNX_INCLUDE_DIR}")
  target_link_libraries(ai_audio_voice_intent_demo PRIVATE ai_audio_assistant)

  add_executable(ai_audio_batch_regression_demo apps/batch_regression_demo.cpp)
  target_include_directories(ai_audio_batch_regression_demo PRIVATE "${LLM_SDK_INCLUDE_DIR}")
  target_link_libraries(ai_audio_batch_regression_demo PRIVATE ai_audio_assistant)
endif()

if(AIAUDIO_BUILD_TESTS)
  enable_testing()
  add_executable(ai_audio_config_test tests/config_test.cpp)
  target_link_libraries(ai_audio_config_test PRIVATE ai_audio_assistant)
  add_test(NAME ai_audio_config_test COMMAND ai_audio_config_test)
endif()

include(GNUInstallDirs)
install(TARGETS ai_audio_assistant
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
