# Boost
set(Boost_NO_WARN_NEW_VERSIONS 1)
find_package(Boost REQUIRED COMPONENTS functional math)

# Eigen
find_package(Eigen3 CONFIG REQUIRED)

# fmt
find_package(fmt CONFIG REQUIRED)

# nlohmann_json
find_package(nlohmann_json CONFIG REQUIRED)

# protobuf
find_package(Protobuf 5.28.3 CONFIG REQUIRED)

# taskflow
find_package(Taskflow CONFIG REQUIRED)

# python
if(BOSIM_BUILD_BINDING)
  find_package(
    Python 3.10
    COMPONENTS Interpreter Development.Module
    REQUIRED)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/externals/nanobind nanobind)
else()
  find_package(
    Python 3.10
    COMPONENTS Interpreter Development.Module Development.Embed
    REQUIRED)
endif()

if(BOSIM_BUILD_SERVER OR BOSIM_BUILD_SERVER_LIB)
  # Boost
  find_package(Boost REQUIRED COMPONENTS program_options)

  # curl
  find_package(CURL REQUIRED)

  # gRPC
  find_package(gRPC CONFIG REQUIRED)

  # zlib
  find_package(ZLIB REQUIRED)
endif()

if(BOSIM_BUILD_TESTS)
  # Google Test
  find_package(GTest CONFIG REQUIRED)
  include(GoogleTest)
endif()

if(BOSIM_DEV_MODE)
  # sanitizer
  set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/externals/sanitizers-cmake/cmake"
                        ${CMAKE_MODULE_PATH})
  find_package(Sanitizers)
endif()
