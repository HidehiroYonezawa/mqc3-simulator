function(set_version VERSION)
  # Get version from Version.txt
  file(STRINGS "${CMAKE_SOURCE_DIR}/Version.txt" VERSION_STR)
  foreach(STR ${VERSION_STR})
    if(${STR} MATCHES "(.*)")
      set(${VERSION}
          "${CMAKE_MATCH_1}"
          PARENT_SCOPE)
    endif()
  endforeach()
endfunction()

function(verify_variable dependent_option target_variable expected_value)
  if(${dependent_option})
    if(NOT "${${target_variable}}" STREQUAL "${expected_value}")
      message(
        FATAL_ERROR
          "'${dependent_option}' requires '${target_variable}' to be ${expected_value}."
      )
    endif()
  endif()
endfunction()
