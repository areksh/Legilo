# Desktop integration on Linux (design 10.3, Appendix C): the desktop file,
# the AppStream metadata and the application icon, under the application
# ID. The tray's icons are compiled into the app (src/app).
if(UNIX AND NOT APPLE)
    install(FILES linux/sh.arek.legilo.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES linux/sh.arek.legilo.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES icons/sh.arek.legilo.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

    # A broken desktop file or metainfo fails here rather than in the
    # package (design 5).
    if(BUILD_TESTING)
        find_program(LEGILO_DESKTOP_FILE_VALIDATE desktop-file-validate REQUIRED)
        find_program(LEGILO_APPSTREAMCLI appstreamcli REQUIRED)
        add_test(NAME desktop_file
            COMMAND ${LEGILO_DESKTOP_FILE_VALIDATE}
                ${CMAKE_CURRENT_SOURCE_DIR}/linux/sh.arek.legilo.desktop)
        add_test(NAME metainfo
            COMMAND ${LEGILO_APPSTREAMCLI} validate --no-net
                ${CMAKE_CURRENT_SOURCE_DIR}/linux/sh.arek.legilo.metainfo.xml)
    endif()
endif()
