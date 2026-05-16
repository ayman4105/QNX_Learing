# ===== QNX Client Build =====
cmake_minimum_required(VERSION 3.10)
project(HelloWorld_Client CXX)
set(CMAKE_CXX_STANDARD 17)

# ── Stage Directory (all libraries and headers are here) ───────────
set(STAGE_DIR "/home/ayman/ITI/QNX/repo/src/stage/usr")

# ── Include Paths ──────────────────────────────────────────────────
set(COMMONAPI_INCLUDE_DIR        ${STAGE_DIR}/include/CommonAPI-3.2)
set(COMMONAPI_SOMEIP_INCLUDE_DIR ${STAGE_DIR}/include/CommonAPI-SomeIP-3.2)
set(VSOMEIP_INCLUDE_DIR          ${STAGE_DIR}/include)

# ── Library Paths ──────────────────────────────────────────────────
set(LIB_DIR ${STAGE_DIR}/lib)

set(COMMONAPI_LIB        ${LIB_DIR}/libCommonAPI.so)
set(COMMONAPI_SOMEIP_LIB ${LIB_DIR}/libCommonAPI-SomeIP.so)
set(VSOMEIP_LIB          ${LIB_DIR}/libvsomeip3.so)

# ── Validate that libraries exist before proceeding ────────────────
foreach(LIB ${COMMONAPI_LIB} ${COMMONAPI_SOMEIP_LIB} ${VSOMEIP_LIB})
    if(NOT EXISTS ${LIB})
        message(FATAL_ERROR "Library not found: ${LIB}")
    endif()
endforeach()

message(STATUS "CommonAPI lib:        ${COMMONAPI_LIB}")
message(STATUS "CommonAPI-SomeIP lib: ${COMMONAPI_SOMEIP_LIB}")
message(STATUS "vsomeip3 lib:         ${VSOMEIP_LIB}")

# ── Generated SOME/IP Sources ──────────────────────────────────────
set(GENERATED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/HelloWorldSomeIPDeployment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/HelloWorldSomeIPProxy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/HelloWorldSomeIPStubAdapter.cpp
)

# ── Executable ────────────────────────────────────────────────────
add_executable(HelloWorldClient
    ${CMAKE_CURRENT_SOURCE_DIR}/SRC/Client.cpp
    ${GENERATED_SOURCES}
)

target_include_directories(HelloWorldClient PRIVATE
    ${COMMONAPI_INCLUDE_DIR}
    ${COMMONAPI_SOMEIP_INCLUDE_DIR}
    ${VSOMEIP_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen
)

target_link_libraries(HelloWorldClient PRIVATE
    ${COMMONAPI_LIB}
    ${COMMONAPI_SOMEIP_LIB}
    ${VSOMEIP_LIB}
)