project(krdc)

add_subdirectory(config)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/rdp/
    ${CMAKE_CURRENT_SOURCE_DIR}/vnc/
)

if(UNIX)
    set(HAVE_RDESKTOP true) # TODO: implement cmake check?
endif(UNIX)

if(HAVE_RDESKTOP)
    add_definitions(-DBUILD_RDP)
    set(krdc_rdp_SRCS
        rdp/rdphostpreferences.cpp
        rdp/rdpview.cpp
    )

    kde4_add_ui_files(krdc_rdp_SRCS
        rdp/rdppreferences.ui
    )
endif(HAVE_RDESKTOP)

if(LIBVNCSERVER_FOUND)
    add_definitions(-DBUILD_VNC)

    include_directories(${LIBVNCCLIENT_INCLUDE_DIR})

    set(krdc_vnc_SRCS
        vnc/vnchostpreferences.cpp
        vnc/vncclientthread.cpp
        vnc/vncview.cpp
    )

    kde4_add_ui_files(krdc_vnc_SRCS
        vnc/vncpreferences.ui
    )
endif(LIBVNCSERVER_FOUND)

set(krdc_SRCS ${krdc_rdp_SRCS} ${krdc_vnc_SRCS}
    hostpreferences.cpp
    config/hostpreferenceslist.cpp
    config/preferencesdialog.cpp
    floatingtoolbar.cpp
    bookmarkmanager.cpp
    remoteview.cpp
    mainwindow.cpp
    main.cpp
)

kde4_add_ui_files(krdc_SRCS
    config/general.ui
)

kde4_add_kcfg_files(krdc_SRCS config/settings.kcfgc)

kde4_add_executable(krdc ${krdc_SRCS})

target_link_libraries(krdc ${KDE4_KFILE_LIBS} ${KDE4_KNOTIFYCONFIG_LIBS})

if(LIBVNCSERVER_FOUND)
    target_link_libraries(krdc ${LIBVNCCLIENT_LIBRARIES})

    if(JPEG_FOUND)
        target_link_libraries(krdc ${JPEG_LIBRARIES})
    endif(JPEG_FOUND)
endif(LIBVNCSERVER_FOUND)

install(TARGETS krdc DESTINATION ${BIN_INSTALL_DIR})
install(FILES krdcui.rc DESTINATION ${DATA_INSTALL_DIR}/krdc)
install(FILES krdc.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install(FILES vnc.protocol rdp.protocol DESTINATION ${SERVICES_INSTALL_DIR})
install(FILES _rfb._tcp DESTINATION ${DATA_INSTALL_DIR}/zeroconf)
install(FILES pointcursor.png pointcursormask.png DESTINATION ${DATA_INSTALL_DIR}/krdc/pics)

kde4_install_icons(${ICON_INSTALL_DIR})
