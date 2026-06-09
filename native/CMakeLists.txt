cmake_minimum_required(VERSION 3.16)
project(flutter_datachannel_native VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(FDC_BUILD_SHARED "Build shared library" ON)
option(FDC_BUILD_SERVER "Build standalone server binary" ON)

include(FetchContent)

# ── libdatachannel ────────────────────────────────────────────────────────
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
set(USE_GNUTLS OFF CACHE BOOL "" FORCE)
set(USE_NICE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    libdatachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG        v0.22.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(libdatachannel)

# ── nlohmann/json ─────────────────────────────────────────────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

set(FDC_SOURCES
    src/fdc_ffi.cpp
    src/signaling_client.cpp
    src/ollama_proxy.cpp
)

if(FDC_BUILD_SHARED)
    add_library(fdc_ffi SHARED ${FDC_SOURCES})
    target_compile_definitions(fdc_ffi PRIVATE FDC_FFI_EXPORTS)
else()
    add_library(fdc_ffi STATIC ${FDC_SOURCES})
endif()

target_include_directories(fdc_ffi
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(fdc_ffi
    PUBLIC
        datachannel
        nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(fdc_ffi PRIVATE ws2_32)
endif()

set_target_properties(fdc_ffi PROPERTIES
    OUTPUT_NAME "flutter_datachannel"
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

# ── Standalone server binary ─────────────────────────────────────────────
if(FDC_BUILD_SERVER)
    add_executable(fdc_server
        server/src/main.cpp
    )
    target_link_libraries(fdc_server PRIVATE fdc_ffi)
    set_target_properties(fdc_server PROPERTIES OUTPUT_NAME "fdc-server")
endif()

# ── Install ───────────────────────────────────────────────────────────────
install(TARGETS fdc_ffi
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES include/fdc_ffi.h DESTINATION include)

if(FDC_BUILD_SERVER)
    install(TARGETS fdc_server RUNTIME DESTINATION bin)
endif()
