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
        rdp/rdpview.cpp
    )

    kde4_add_ui_files(krdc_rdp_SRCS
        rdp/rdppreferences.ui
    )
endif(HAVE_RDESKTOP)

check_symbol_exists(rfbInitServer "rfb/rfb.h" HAVE_LIBVNCSERVER)

if(HAVE_LIBVNCSERVER)
    add_definitions(-DBUILD_VNC)
    set(krdc_vnc_SRCS
        vnc/vnchostpreferences.cpp
        vnc/vncclientthread.cpp
        vnc/vncview.cpp
    )

    kde4_add_ui_files(krdc_vnc_SRCS
        vnc/vncpreferences.ui
    )
endif(HAVE_LIBVNCSERVER)

set(krdc_SRCS ${krdc_rdp_SRCS} ${krdc_vnc_SRCS}
    hostpreferences.cpp
    config/preferencesdialog.cpp
    floatingtoolbar.cpp
    remoteview.cpp
    mainwindow.cpp
    main.cpp
)

kde4_add_ui_files(krdc_SRCS
    config/general.ui
)

kde4_automoc(${krdc_SRCS})

kde4_add_kcfg_files(krdc_SRCS config/settings.kcfgc)

kde4_add_executable(krdc ${krdc_SRCS})

target_link_libraries(krdc ${KDE4_KDEUI_LIBRARY} ${KDE4_KFILE_LIBRARY} ${KDE4_KNOTIFYCONFIG_LIBRARY})

if(HAVE_LIBVNCSERVER)
    target_link_libraries(krdc ${LIBVNCCLIENT_LIBRARIES})
endif(HAVE_LIBVNCSERVER)

install(TARGETS krdc DESTINATION ${BIN_INSTALL_DIR})
install(FILES krdcui.rc DESTINATION ${DATA_INSTALL_DIR}/krdc)

kde4_install_icons(${ICON_INSTALL_DIR})
