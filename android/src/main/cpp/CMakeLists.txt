# Flutter Android FFI plugin
cmake_minimum_required(VERSION 3.14)
project(flutter_datachannel_plugin LANGUAGES CXX)

set(FDC_BUILD_SHARED ON CACHE BOOL "" FORCE)
set(FDC_BUILD_SERVER OFF CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../native" fdc_native)

# Android Gradle expects the .so name without lib prefix in some versions;
# OUTPUT_NAME is set to flutter_datachannel in native/CMakeLists.txt.
set(flutter_datachannel_bundled_libraries
  $<TARGET_FILE:fdc_ffi>
  PARENT_SCOPE
)
