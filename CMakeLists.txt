cmake_minimum_required(VERSION 3.18)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(utils)
set_version(VERSION)

project(
  bosim
  VERSION ${VERSION}
  DESCRIPTION "Bosonic simulator."
  LANGUAGES CXX)
set(PROJECT_NAMESPACE bosim)
message(STATUS "${PROJECT_NAME} version: ${PROJECT_VERSION}")

#
# Use the C++20 standard.
#
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# Set general options.
#
option(BOSIM_DEV_MODE "Build in developer mode." OFF)
option(BOSIM_USE_GPU "Build gpu codes." OFF)
set(BOSIM_CUDA_ARCHITECTURES
    "86;90"
    CACHE
      STRING
      "CUDA architectures for bosim (e.g. 90;86;80). Empty = use CMake default")
option(BOSIM_BUILD_EXAMPLES "Build examples." OFF)
option(BOSIM_BUILD_SERVER "Build bosim grpc server." OFF)
option(BOSIM_BUILD_SERVER_LIB "Build library part of bosim grpc server." OFF)
option(BOSIM_BUILD_TESTS "Build tests." OFF)
option(BOSIM_BUILD_BENCHMARK "Build benchmark." OFF)
option(BOSIM_BUILD_BINDING "Build Python binding." OFF)
option(BOSIM_MEASURE_COVERAGE "Measure coverage." OFF)
message(STATUS "bosim general options.")
message(STATUS "  * BOSIM_DEV_MODE           : ${BOSIM_DEV_MODE}")
message(STATUS "  * BOSIM_USE_GPU            : ${BOSIM_USE_GPU}")
message(STATUS "  * BOSIM_CUDA_ARCHITECTURES : ${BOSIM_CUDA_ARCHITECTURES}")
message(STATUS "  * BOSIM_BUILD_EXAMPLES     : ${BOSIM_BUILD_EXAMPLES}")
message(STATUS "  * BOSIM_BUILD_SERVER       : ${BOSIM_BUILD_SERVER}")
message(STATUS "  * BOSIM_BUILD_SERVER_LIB   : ${BOSIM_BUILD_SERVER_LIB}")
message(STATUS "  * BOSIM_BUILD_TESTS        : ${BOSIM_BUILD_TESTS}")
message(STATUS "  * BOSIM_BUILD_BENCHMARK    : ${BOSIM_BUILD_BENCHMARK}")
message(STATUS "  * BOSIM_BUILD_BINDING      : ${BOSIM_BUILD_BINDING}")
message(STATUS "  * BOSIM_MEASURE_COVERAGE   : ${BOSIM_MEASURE_COVERAGE}")

#
# Set default build type to be 'Release'.
#
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      "Release"
      CACHE
        STRING
        "Choose the type of build, options are: Debug Release. (default: Release)"
        FORCE)
endif()

#
# Check options.
#
verify_variable(BOSIM_BUILD_SERVER CMAKE_BUILD_TYPE "Release")
verify_variable(BOSIM_BUILD_SERVER_LIB CMAKE_BUILD_TYPE "Release")
verify_variable(BOSIM_MEASURE_COVERAGE CMAKE_BUILD_TYPE "Debug")
verify_variable(BOSIM_MEASURE_COVERAGE BOSIM_USE_GPU "OFF")
verify_variable(BOSIM_MEASURE_COVERAGE BOSIM_BUILD_SERVER "OFF")
verify_variable(BOSIM_MEASURE_COVERAGE BOSIM_BUILD_SERVER_LIB "OFF")
verify_variable(BOSIM_MEASURE_COVERAGE BOSIM_BUILD_TESTS "ON")
verify_variable(BOSIM_BUILD_BINDING BOSIM_BUILD_EXAMPLES "OFF")
verify_variable(BOSIM_BUILD_BINDING BOSIM_BUILD_SERVER "OFF")
verify_variable(BOSIM_BUILD_BINDING BOSIM_BUILD_SERVER_LIB "OFF")
verify_variable(BOSIM_BUILD_BINDING BOSIM_BUILD_TESTS "OFF")
verify_variable(BOSIM_BUILD_BINDING BOSIM_BUILD_BENCHMARK "OFF")

#
# Include deps.cmake.
#
include(deps)

#
# Configure global options.
#
# Always generate compile_commands.json.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#
# Configure options.
#
if(BOSIM_USE_GPU)
  include(gpu)
endif()
if(BOSIM_BUILD_TESTS)
  enable_testing()
endif()

#
# Configure version header.
#
configure_file(cmake/version.h.in src/bosim/version.h)

#
# Settings for developer.
#
if(BOSIM_DEV_MODE)
  find_program(CCACHE_PROGRAM ccache)

  if(CCACHE_PROGRAM)
    message(STATUS "Enable ccache.")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  else()
    message(WARNING "ccache not found.")
  endif()
endif()

#
# Add subdirectories.
#
add_subdirectory(codegen)
add_subdirectory(src)

if(BOSIM_BUILD_SERVER OR BOSIM_BUILD_SERVER_LIB)
  add_subdirectory(server)
endif()

if(BOSIM_BUILD_TESTS)
  add_subdirectory(tests)
endif()

if(BOSIM_BUILD_BENCHMARK)
  add_subdirectory(benchmark)
endif()

if(BOSIM_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

#
# Configure install settings.
#
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
configure_package_config_file(
  cmake/${PROJECT_NAME}Config.cmake.in
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  NO_CHECK_REQUIRED_COMPONENTS_MACRO)
install(
  FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  COMPONENT Devel)
install(
  TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}Targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
  EXPORT ${PROJECT_NAME}Targets
  NAMESPACE ${PROJECT_NAMESPACE}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
install(
  DIRECTORY src/bosim codegen/mqc3_cloud
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT Devel
  FILES_MATCHING
  PATTERN "*.h")
install(FILES "${PROJECT_BINARY_DIR}/src/bosim/version.h"
        DESTINATION include/bosim)
