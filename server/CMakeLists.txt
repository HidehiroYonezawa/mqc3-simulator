if(BOSIM_BUILD_SERVER_LIB)
  add_subdirectory(src)
endif()

if(BOSIM_BUILD_SERVER)
  list(
    APPEND
    BOSIM_SERVER_SOURCE_FILES
    # common
    src/server/common/message_manager.cpp
    src/server/common/utility.cpp
    # runner
    src/server/runner/validate.cpp
    # server
    src/server/server/run_server.cpp
    src/server/server/server.cpp
    # main
    main/main.cpp)

  add_executable(bosim_server ${BOSIM_SERVER_SOURCE_FILES}
                              ${BOSIM_CODEGEN_SERVER_SOURCE_FILES})
  target_include_directories(
    bosim_server PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
                        $<BUILD_INTERFACE:${BOSIM_CODEGEN_INCLUDE_DIR}>)
  target_link_libraries(
    bosim_server
    PRIVATE ${PROJECT_NAME}
            bosim
            Boost::program_options
            CURL::libcurl
            gRPC::grpc++
            gRPC::grpc++_reflection
            ZLIB::ZLIB)
  set_target_properties(
    bosim_server
    PROPERTIES CXX_STANDARD 20
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)
endif()
