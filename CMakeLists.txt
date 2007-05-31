project(krdc)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/rdp/
    ${CMAKE_CURRENT_SOURCE_DIR}/vnc/
)

set(krdc_rdp_SRCS
    rdp/rdpview.cpp
)

check_symbol_exists(rfbInitServer "rfb/rfb.h" HAVE_LIBVNCSERVER)

if(HAVE_LIBVNCSERVER)
    add_definitions(-DBUILDVNC)
    set(krdc_vnc_SRCS
        vnc/vncclientthread.cpp
        vnc/vncview.cpp
    )
endif(HAVE_LIBVNCSERVER)

set(krdc_SRCS ${krdc_rdp_SRCS} ${krdc_vnc_SRCS}
    remoteview.cpp
    mainwindow.cpp
    main.cpp
)

kde4_automoc(${krdc_SRCS})

kde4_add_executable(krdc ${krdc_SRCS})

target_link_libraries(krdc ${KDE4_KDEUI_LIBRARY} ${KDE4_KNOTIFYCONFIG_LIBRARY})

if(HAVE_LIBVNCSERVER)
    target_link_libraries(krdc ${LIBVNCCLIENT_LIBRARIES})
endif(HAVE_LIBVNCSERVER)

install(TARGETS krdc DESTINATION ${BIN_INSTALL_DIR})
install(FILES krdcui.rc DESTINATION ${DATA_INSTALL_DIR}/krdc)

kde4_install_icons(${ICON_INSTALL_DIR})
