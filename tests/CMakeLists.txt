function(add_test directory filename)
  string(REPLACE "/" "_" directory_name ${directory})
  set(target ${directory_name}_${filename})
  add_executable(${target} ${directory}/${filename}.cpp)
  target_link_libraries(${target} PRIVATE GTest::gtest GTest::gtest_main
                                          ${PROJECT_NAME})
  set_target_properties(
    ${target}
    PROPERTIES CXX_STANDARD 20
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)

  if(BOSIM_MEASURE_COVERAGE)
    target_compile_options(
      ${target} PRIVATE -g -fprofile-instr-generate -fcoverage-mapping
                        -fclang-abi-compat=17)
    target_link_options(${target} PRIVATE -g -fprofile-instr-generate
                        -fcoverage-mapping -fclang-abi-compat=17)
  endif()

  gtest_discover_tests(${target} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                                                   DISCOVERY_MODE PRE_TEST)
endfunction()

function(add_server_test directory filename)
  string(REPLACE "/" "_" directory_name ${directory})
  set(target ${directory_name}_${filename})
  add_executable(${target} ${directory}/${filename}.cpp)
  target_link_libraries(${target} PRIVATE GTest::gtest GTest::gtest_main
                                          ${PROJECT_NAME} bosim_server_lib)
  set_target_properties(
    ${target}
    PROPERTIES CXX_STANDARD 20
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)

  gtest_discover_tests(${target} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                                                   DISCOVERY_MODE PRE_TEST)
endfunction()

function(add_exe directory filename)
  string(REPLACE "/" "_" directory_name ${directory})
  set(target ${directory_name}_${filename})
  add_executable(${target} ${directory}/${filename}.cpp)
  target_link_libraries(${target} PRIVATE ${PROJECT_NAME})
  set_target_properties(
    ${target}
    PROPERTIES CXX_STANDARD 20
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)
endfunction()

target_include_directories(
  ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                         $<INSTALL_INTERFACE:include>)

# base
add_test(base log)
add_test(base math)
add_test(base state)
add_test(base operation)
add_test(base circuit)
add_test(base parallel)

# circuit
add_test(circuit circuit_repr)
add_test(circuit circuit_result)
add_test(circuit teleportation)

# graph
add_test(graph graph_repr)
add_test(graph graph_result)
add_test(graph graph_simulate)

# cli
add_exe(cli circuit)
add_exe(cli graph)
add_exe(cli machinery)

# machinery
add_test(machinery machinery_repr)
add_test(machinery machinery_simulate)

# GPU status
add_test(gpu status)

if(BOSIM_BUILD_SERVER OR BOSIM_BUILD_SERVER_LIB)
  # python
  add_test(python function)
endif()

if(BOSIM_BUILD_SERVER_LIB)
  # server
  add_server_test(server/common message_manager)
  add_server_test(server/common utility)

  add_server_test(server/runner runner)
endif()

if(BOSIM_USE_GPU)
  # gpu
  add_test(gpu circuit)
endif()
