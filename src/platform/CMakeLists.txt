# Abstract interfaces live in include/platform/; the implementations for
# the target platform are added to this library from linux/ or windows/.
add_library(legilo_platform STATIC
    Logging.cpp
    include/platform/GlobalShortcuts.h
    include/platform/Logging.h
    include/platform/Platform.h
    include/platform/TextSource.h
)
target_include_directories(legilo_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(legilo_platform PUBLIC legilo_core Qt6::Core Qt6::Gui)

# KDE Frameworks are linked only in the Linux build (6.1): KGlobalAccel for
# the global shortcuts (7.6.3) and KSystemClipboard, from GuiAddons, for
# reading the selection on Wayland (7.1.2).
if(UNIX AND NOT APPLE)
    find_package(KF6GlobalAccel 6.13 REQUIRED)
    find_package(KF6GuiAddons 6.13 REQUIRED)
    target_sources(legilo_platform PRIVATE
        linux/KdeTextSource.cpp
        linux/KdeTextSource.h
        linux/KGlobalAccelShortcuts.cpp
        linux/KGlobalAccelShortcuts.h
        linux/Platform.cpp
    )
    target_include_directories(legilo_platform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(legilo_platform PRIVATE KF6::GlobalAccel KF6::GuiAddons)
endif()
legilo_set_warnings(legilo_platform)
