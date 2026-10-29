cmake_minimum_required(VERSION 3.25)

project(legilo
    VERSION 0.0.0
    DESCRIPTION "Offline text-to-speech reader"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LEGILO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(BUILD_TESTING "Build the tests" ON)

# Shared code must stay within the Qt 6.8 API that Debian trixie ships,
# even where Windows builds against a newer Qt.
find_package(Qt6 6.8 REQUIRED COMPONENTS Core)
qt_standard_project_setup(REQUIRES 6.8)
add_compile_definitions(QT_DISABLE_DEPRECATED_UP_TO=0x060800)

include(cmake/CompilerWarnings.cmake)
include(cmake/SherpaOnnx.cmake)

add_subdirectory(src)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
