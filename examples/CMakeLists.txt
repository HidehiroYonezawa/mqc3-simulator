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

add_exe(teleportation)
