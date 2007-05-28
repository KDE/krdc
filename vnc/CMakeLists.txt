check_symbol_exists(rfbInitServer "rfb/rfb.h" HAVE_LIBVNCSERVER)

include_directories(
    ${LIBVNCSERVER_INCLUDE_DIR}
)

if(HAVE_LIBVNCSERVER)

set(krdc-vnc_SRCS
    vncclientthread.cpp
    vncview.cpp
    main.cpp
)

kde4_automoc(${krdc-vnc_SRCS})

kde4_add_executable(krdc-vnc ${krdc-vnc_SRCS})

target_link_libraries(krdc-vnc ${QT_QTGUI_LIBRARY} ${LIBVNCCLIENT_LIBRARIES})

install(TARGETS krdc-vnc DESTINATION ${BIN_INSTALL_DIR})

endif(HAVE_LIBVNCSERVER)
