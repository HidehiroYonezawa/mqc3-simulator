# Check CUDA installed.
include(CheckLanguage)
check_language(CUDA)
enable_language(CUDA)

if(NOT CMAKE_CUDA_COMPILER)
  message(FATAL_ERROR "No CUDA support.")
endif()
