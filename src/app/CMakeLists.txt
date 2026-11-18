# Everything except main() goes in a library so the tests can link it.
add_library(legilo_app STATIC
    CommandLine.cpp
    CommandLine.h
    Instance.cpp
    Instance.h
    IpcProtocol.cpp
    IpcProtocol.h
    Logging.cpp
    Logging.h
    Settings.cpp
    Settings.h
    SingleInstance.cpp
    SingleInstance.h
    Speaker.cpp
    Speaker.h
    TrayController.cpp
    TrayController.h
)
target_include_directories(legilo_app PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(legilo_app PUBLIC legilo_core legilo_platform Qt6::Widgets Qt6::Network)
legilo_set_warnings(legilo_app)

# The application and tray icons, compiled in so the tray has them however
# Legilo is run, installed or from a build tree. Qt draws them through its
# SVG image plugin (qt6-svg-plugins on Debian, qt6-qtsvg on Fedora).
set(legilo_icons
    ${PROJECT_SOURCE_DIR}/resources/icons/sh.arek.legilo.svg
    ${PROJECT_SOURCE_DIR}/resources/icons/tray-speaking.svg
    ${PROJECT_SOURCE_DIR}/resources/icons/tray-paused.svg)
foreach(icon IN LISTS legilo_icons)
    get_filename_component(name ${icon} NAME)
    set_source_files_properties(${icon} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()
qt_add_resources(legilo_app legilo_icons PREFIX /legilo/icons FILES ${legilo_icons})

add_executable(legilo main.cpp)
target_compile_definitions(legilo PRIVATE LEGILO_VERSION="${PROJECT_VERSION}")
target_link_libraries(legilo PRIVATE legilo_app)
legilo_set_warnings(legilo)

# sherpa-onnx's libraries are private to Legilo, in lib/legilo (10.1).
if(UNIX)
    set_target_properties(legilo PROPERTIES INSTALL_RPATH "$ORIGIN/../lib/legilo")
endif()

install(TARGETS legilo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
