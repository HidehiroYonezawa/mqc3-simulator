list(
  APPEND
  BOSIM_SERVER_LIB_SOURCE_FILES
  # common
  server/common/message_manager.cpp
  server/common/utility.cpp
  # runner
  server/runner/validate.cpp
  # server
  server/server/run_server.cpp
  server/server/server.cpp
  #
)

add_library(bosim_server_lib STATIC ${BOSIM_SERVER_LIB_SOURCE_FILES}
                                    ${BOSIM_CODEGEN_SERVER_SOURCE_FILES})
target_include_directories(
  bosim_server_lib PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                          $<BUILD_INTERFACE:${BOSIM_CODEGEN_INCLUDE_DIR}>)

target_link_libraries(
  bosim_server_lib PRIVATE ${PROJECT_NAME} bosim CURL::libcurl gRPC::grpc++
                           gRPC::grpc++_reflection ZLIB::ZLIB)
set_target_properties(
  bosim_server_lib
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF)
