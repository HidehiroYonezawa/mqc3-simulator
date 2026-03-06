#
# Set compile options.
#
if(MSVC)
  list(APPEND BOSIM_COMPILE_OPTIONS "/W4" "/Zc:preprocessor")
else()
  list(
    APPEND
    BOSIM_COMPILE_OPTIONS
    "-Wall"
    "-Wextra"
    "-Wcast-align"
    "-Wcast-qual"
    "-Wdelete-non-virtual-dtor"
    # "-Wold-style-cast"
    "-Woverloaded-virtual"
    "-Wpointer-arith"
    "-Wfloat-equal"
    "-Winit-self"
    "-Wredundant-decls"
    "-Wconversion"
    "-Wsign-promo"
    "-Wswitch-default"
    "-Wvariadic-macros"
    "-Wwrite-strings"
    "-Wno-unknown-pragmas")
endif()

#
# Set compile definitions.
#
if(BOSIM_USE_GPU)
  list(APPEND BOSIM_COMPILE_DEFINITIONS "BOSIM_USE_GPU")
endif()

if(BOSIM_BUILD_BENCHMARK)
  list(APPEND BOSIM_COMPILE_DEFINITIONS "BOSIM_BUILD_BENCHMARK")
endif()

#
# Set sources to compile.
#
list(
  APPEND
  BOSIM_SOURCE_FILES
  # base.
  bosim/base/log.cpp
  # simulate.
  bosim/simulate/simulate.cpp
  #
)
if(BOSIM_USE_GPU)
  list(
    APPEND
    BOSIM_SOURCE_FILES
    # gpu.
    bosim/gpu/simulate_gpu.cu
    bosim/gpu/status.cu
    #
  )
endif()

#
# Define target.
#
message(STATUS "Build ${PROJECT_NAME} as static library")
add_library(${PROJECT_NAME} STATIC ${BOSIM_SOURCE_FILES}
                                   ${BOSIM_CODEGEN_PROGRAM_SOURCE_FILES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

#
# Define target properties.
#
target_include_directories(
  ${PROJECT_NAME}
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         $<BUILD_INTERFACE:${BOSIM_CODEGEN_INCLUDE_DIR}>
         $<INSTALL_INTERFACE:include>
         ${Python_INCLUDE_DIRS})
target_link_libraries(
  ${PROJECT_NAME}
  PUBLIC Boost::functional
         Boost::math
         Eigen3::Eigen
         fmt::fmt-header-only
         nlohmann_json::nlohmann_json
         protobuf::libprotobuf
         Taskflow::Taskflow
         ${Python_LIBRARIES})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  # Add the following compile option -fclang-abi-compat=17 only for Debug
  # builds. This is a temporary workaround for a Clang bug described in
  # https://github.com/llvm/llvm-project/issues/102443.
  target_compile_options(
    ${PROJECT_NAME}
    PUBLIC $<$<BUILD_INTERFACE:$<COMPILE_LANGUAGE:CXX>>:-fclang-abi-compat=17>)
endif()

if(BOSIM_USE_GPU)
  # Include CUDA Toolkit.
  target_include_directories(${PROJECT_NAME} SYSTEM
                             PUBLIC ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})

  set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
      VERSION
      ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
      POSITION_INDEPENDENT_CODE ON
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF
      CUDA_STANDARD 20
      CUDA_STANDARD_REQUIRED ON
      CUDA_EXTENSIONS OFF
      CUDA_SEPARABLE_COMPILATION ON
      CUDA_RESOLVE_DEVICE_SYMBOLS ON
      CUDA_ARCHITECTURES "${BOSIM_CUDA_ARCHITECTURES}")
  target_compile_definitions(${PROJECT_NAME}
                             PUBLIC ${BOSIM_COMPILE_DEFINITIONS})
  target_compile_options(
    ${PROJECT_NAME}
    PUBLIC # CXX, CUDA
           ${BOSIM_COMPILE_OPTIONS}
           # CUDA
           $<$<BUILD_INTERFACE:$<COMPILE_LANGUAGE:CUDA>>:
           -use_fast_math
           -Xptxas=-v>)
else()
  set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
      VERSION
      ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
      POSITION_INDEPENDENT_CODE ON
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF)
  target_compile_definitions(${PROJECT_NAME}
                             PUBLIC ${BOSIM_COMPILE_DEFINITIONS})
  target_compile_options(${PROJECT_NAME} PUBLIC ${BOSIM_COMPILE_OPTIONS})
endif()

if(BOSIM_MEASURE_COVERAGE)
  target_compile_options(
    ${PROJECT_NAME} PRIVATE -g -fprofile-instr-generate -fcoverage-mapping
                            -fclang-abi-compat=17)
  target_link_options(${PROJECT_NAME} PRIVATE -g -fprofile-instr-generate
                      -fcoverage-mapping -fclang-abi-compat=17)
endif()

#
# Add sanitizers.
#
if(BOSIM_DEV_MODE)
  add_sanitizers(${PROJECT_NAME})
endif()

#
# Python binding (_mqc3_simulator_impl).
#
if(BOSIM_BUILD_BINDING)
  nanobind_add_module(
    _mqc3_simulator_impl NB_SUPPRESS_WARNINGS NB_STATIC STABLE_ABI
    ${CMAKE_CURRENT_SOURCE_DIR}/bosim/binding/simulator_binding.cpp)
  target_link_libraries(_mqc3_simulator_impl PRIVATE ${PROJECT_NAME})
  if(APPLE)
    target_link_options(_mqc3_simulator_impl PRIVATE
                        "-Wl,-undefined,dynamic_lookup")
  endif()
  nanobind_set_visibility(_mqc3_simulator_impl)
  nanobind_strip(_mqc3_simulator_impl)
  install(
    TARGETS _mqc3_simulator_impl
    DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/mqc3_simulator"
    COMPONENT python_module)
endif()
