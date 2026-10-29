# sherpa-onnx, the speech engine, built as a separate CMake project from the
# pinned archive in third_party/downloads/ (cmake/Dependencies.cmake).
#
# It is an ExternalProject rather than part of this build because its
# top-level CMakeLists.txt expects to be the top level: it forces a release
# build, sets global output directories and definitions, and installs its
# headers, static libraries and pkg-config files. Built apart, it installs
# into the build tree, and Legilo installs only the two shared libraries it
# links, as private libraries in lib/legilo.
#
# On Linux it links Debian's ONNX Runtime (libonnxruntime-dev) instead of
# downloading the prebuilt one sherpa-onnx defaults to. The Windows build
# is set up in stage 5.
#
# Provides the imported target Legilo::SherpaOnnx (the C++ API).

include(ExternalProject)
include(${CMAKE_CURRENT_LIST_DIR}/Dependencies.cmake)

set(LEGILO_DOWNLOADS_DIR "${PROJECT_SOURCE_DIR}/third_party/downloads"
    CACHE PATH "Directory holding the archives from scripts/fetch-deps")

set(archives "")
foreach(entry IN LISTS LEGILO_DEPENDENCIES)
    legilo_dependency_fields("${entry}" name url hash)
    if(NOT EXISTS "${LEGILO_DOWNLOADS_DIR}/${name}")
        message(FATAL_ERROR
            "${LEGILO_DOWNLOADS_DIR}/${name} is missing. Run scripts/fetch-deps.")
    endif()
    if(name STREQUAL LEGILO_SHERPA_ONNX_ARCHIVE)
        set(sherpa_hash "${hash}")
    else()
        list(APPEND archives "${LEGILO_DOWNLOADS_DIR}/${name}")
    endif()
endforeach()

# Check here, because sherpa-onnx silently downloads its own ONNX Runtime
# when it finds none installed.
find_path(LEGILO_ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h PATH_SUFFIXES onnxruntime)
find_library(LEGILO_ONNXRUNTIME_LIBRARY onnxruntime)
if(NOT LEGILO_ONNXRUNTIME_INCLUDE_DIR OR NOT LEGILO_ONNXRUNTIME_LIBRARY)
    message(FATAL_ERROR "ONNX Runtime not found. Install libonnxruntime-dev.")
endif()

set(sherpa_install "${CMAKE_BINARY_DIR}/_deps/sherpa-onnx/install")
set(sherpa_c_api "${sherpa_install}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}sherpa-onnx-c-api${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(sherpa_cxx_api "${sherpa_install}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}sherpa-onnx-cxx-api${CMAKE_SHARED_LIBRARY_SUFFIX}")

ExternalProject_Add(sherpa_onnx_external
    URL "${LEGILO_DOWNLOADS_DIR}/${LEGILO_SHERPA_ONNX_ARCHIVE}"
    URL_HASH SHA256=${sherpa_hash}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX "${CMAKE_BINARY_DIR}/_deps/sherpa-onnx"
    INSTALL_DIR "${sherpa_install}"
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DBUILD_SHARED_LIBS=ON
        -DSHERPA_ONNX_USE_PRE_INSTALLED_ONNXRUNTIME_IF_AVAILABLE=ON
        -DSHERPA_ONNX_ENABLE_BINARY=OFF
        -DSHERPA_ONNX_BUILD_C_API_EXAMPLES=OFF
        -DSHERPA_ONNX_ENABLE_JNI=OFF
        -DSHERPA_ONNX_ENABLE_PORTAUDIO=OFF
        -DSHERPA_ONNX_ENABLE_PYTHON=OFF
        -DSHERPA_ONNX_ENABLE_SPEAKER_DIARIZATION=OFF
        -DSHERPA_ONNX_ENABLE_TESTS=OFF
        -DSHERPA_ONNX_ENABLE_WEBSOCKET=OFF
    BUILD_BYPRODUCTS "${sherpa_c_api}" "${sherpa_cxx_api}"
    LOG_CONFIGURE TRUE
    LOG_BUILD TRUE
    LOG_INSTALL TRUE
    LOG_OUTPUT_ON_FAILURE TRUE
)

# Put the other archives where sherpa-onnx's configure step looks for them.
ExternalProject_Add_Step(sherpa_onnx_external local_archives
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${archives} <BINARY_DIR>
    DEPENDEES download
    DEPENDERS configure
    DEPENDS ${archives}
)

# An imported target's include directory must exist when CMake generates
# the build, before sherpa-onnx has installed anything into it.
file(MAKE_DIRECTORY "${sherpa_install}/include")

add_library(legilo_sherpa_onnx_c_api SHARED IMPORTED GLOBAL)
set_target_properties(legilo_sherpa_onnx_c_api PROPERTIES
    IMPORTED_LOCATION "${sherpa_c_api}"
    INTERFACE_INCLUDE_DIRECTORIES "${sherpa_install}/include")
add_dependencies(legilo_sherpa_onnx_c_api sherpa_onnx_external)

add_library(legilo_sherpa_onnx_cxx_api SHARED IMPORTED GLOBAL)
set_target_properties(legilo_sherpa_onnx_cxx_api PROPERTIES
    IMPORTED_LOCATION "${sherpa_cxx_api}"
    INTERFACE_LINK_LIBRARIES legilo_sherpa_onnx_c_api)
add_dependencies(legilo_sherpa_onnx_cxx_api sherpa_onnx_external)

add_library(Legilo::SherpaOnnx ALIAS legilo_sherpa_onnx_cxx_api)

install(FILES "${sherpa_c_api}" "${sherpa_cxx_api}" DESTINATION lib/legilo)
