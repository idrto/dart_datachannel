# Flutter iOS FFI plugin
cmake_minimum_required(VERSION 3.14)
project(flutter_datachannel_plugin LANGUAGES CXX)

set(FDC_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(FDC_BUILD_SERVER OFF CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../native" fdc_native)

# Static library linked into the iOS app binary.
target_compile_definitions(fdc_ffi PUBLIC FDC_FFI_EXPORTS)

set(flutter_datachannel_bundled_libraries
  ""
  PARENT_SCOPE
)

# Expose static lib to the iOS build via plugin CMake hook.
set(FDC_IOS_STATIC_LIB fdc_ffi PARENT_SCOPE)
