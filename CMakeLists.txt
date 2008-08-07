project(krdc)

add_subdirectory(config)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/rdp/
    ${CMAKE_CURRENT_SOURCE_DIR}/vnc/
    ${CMAKE_CURRENT_SOURCE_DIR}/nx/
    ${CMAKE_CURRENT_SOURCE_DIR}/zeroconf/
)

if(Q_WS_X11)
    set(HAVE_RDESKTOP true) # TODO: implement cmake check?
endif(Q_WS_X11)

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

if(DNSSD_FOUND)
    add_definitions(-DBUILD_ZEROCONF)
    set(krdc_zeroconf_SRCS
        zeroconf/zeroconfpage.cpp
    )
endif(DNSSD_FOUND)

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

if(LIBNXCL_FOUND)
    add_definitions(-DBUILD_NX)

    include_directories(${LIBNXCL_INCLUDE_DIR})

    set(krdc_nx_SRCS
        nx/nxhostpreferences.cpp
	nx/nxview.cpp
	nx/nxclientthread.cpp
    )

    kde4_add_ui_files(krdc_nx_SRCS
        nx/nxpreferences.ui
    )
endif(LIBNXCL_FOUND)

set(krdc_SRCS ${krdc_zeroconf_SRCS} ${krdc_rdp_SRCS} ${krdc_vnc_SRCS} ${krdc_nx_SRCS}
    hostpreferences.cpp
    config/hostpreferenceslist.cpp
    config/preferencesdialog.cpp
    floatingtoolbar.cpp
    bookmarkmanager.cpp
    remotedesktopsitem.cpp
    remotedesktopsmodel.cpp
    systemtrayicon.cpp
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

if(DNSSD_FOUND)
   target_link_libraries(krdc ${KDE4_KDNSSD_LIBS})
endif(DNSSD_FOUND)

if(LIBNXCL_FOUND)
   target_link_libraries(krdc ${LIBNXCL_LIBRARIES})
endif(LIBNXCL_FOUND)

install(TARGETS krdc ${INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES krdcui.rc DESTINATION ${DATA_INSTALL_DIR}/krdc)
install(FILES krdc.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install(FILES vnc.protocol rdp.protocol DESTINATION ${SERVICES_INSTALL_DIR})
install(FILES smb2rdc.desktop DESTINATION ${SERVICES_INSTALL_DIR}/ServiceMenus)
install(FILES pointcursor.png pointcursormask.png DESTINATION ${DATA_INSTALL_DIR}/krdc/pics)

if(LIBNXCL_FOUND)
   install(FILES nx/default.dsa.key DESTINATION ${DATA_INSTALL_DIR}/krdc)
endif(LIBNXCL_FOUND)

