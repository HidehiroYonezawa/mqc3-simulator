function(add_exe filename)
  set(target ${filename})
  add_executable(${target} ${filename}.cpp)
  target_link_libraries(${target} PRIVATE ${PROJECT_NAME})
  set_target_properties(
    ${target}
    PROPERTIES CXX_STANDARD 20
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)
endfunction()

add_exe(table_header)
add_exe(rotation_op_time)
add_exe(bs_op_time)
add_exe(measure_op_time)
add_exe(initial_state_time)
add_exe(initial_state_time_gpu)

add_subdirectory(graph)
add_subdirectory(machinery)
