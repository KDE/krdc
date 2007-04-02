project(krdc)
macro_bool_to_01(X11VIDMODE_FOUND HAVE_VIDMODE_EXTENSION)

configure_file (config-krdc.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-krdc.h )

########### next target ###############

include_directories(
${JPEG_INCLUDE_DIR}
${CMAKE_CURRENT_SOURCE_DIR}/rdp/
${CMAKE_CURRENT_SOURCE_DIR}/vnc/
)


set(krdc_rdp_SRCS
    rdp/krdpview.cpp
    rdp/rdphostpref.cpp
    rdp/rdpprefs.cpp
 )

set(krdc_vnc_SRCS
    vnc/kvncview.cpp 
    vnc/threads.cpp 
    vnc/colour.c 
    vnc/d3des.c 
    vnc/desktop.c 
    vnc/rfbproto.c 
    vnc/sockets.c 
    vnc/vncauth.c 
    vnc/vnchostpref.cpp 
    vnc/vncprefs.cpp
)


set(krdc_SRCS ${krdc_vnc_SRCS} ${krdc_rdp_SRCS}
   main.cpp 
   krdc.cpp 
   vidmode.cpp 
   kfullscreenpanel.cpp 
   kservicelocator.cpp 
   keycapturedialog.cpp 
   kremoteview.cpp 
   smartptr.cpp 
   hostpreferences.cpp 
   preferencesdialog.cpp 
   maindialogwidget.cpp 
   maindialog.cpp
   hostprofiles.cpp 
)

kde4_automoc(${krdc_SRCS})

kde4_add_ui_files(krdc_SRCS hostprofiles.ui maindialogbase.ui vnc/vncprefs.ui rdp/rdpprefs.ui)
kde4_add_ui3_files(krdc_SRCS keycapturewidget.ui)


kde4_add_executable(krdc ${krdc_SRCS})

target_link_libraries(krdc  ${JPEG_LIBRARIES} ${KDE4_KDEUI_LIBS} ${KDE4_KDNSSD_LIBS} ${KDE4_KWALLETCLIENT_LIBS} ${SLP_LIBRARIES} ${KDE4_KDE3SUPPORT_LIBS})
if (HAVE_VIDMODE_EXTENSION)
  target_link_libraries(krdc ${X11VIDMODE_LIBRARIES})
endif (HAVE_VIDMODE_EXTENSION)

install(TARGETS krdc  DESTINATION ${BIN_INSTALL_DIR} )


########### install files ###############

install( FILES krdc.desktop  DESTINATION ${XDG_APPS_DIR})
install( FILES smb2rdc.desktop  DESTINATION ${DATA_INSTALL_DIR}/konqueror/servicemenus)
install( FILES pointcursor.png pointcursormask.png pinup.png pindown.png iconify.png DESTINATION ${DATA_INSTALL_DIR}/krdc/pics)
install( FILES vnc.protocol rdp.protocol  DESTINATION ${SERVICES_INSTALL_DIR})
install( FILES _rfb._tcp  DESTINATION ${DATA_INSTALL_DIR}/zeroconf)

kde4_install_icons( ${ICON_INSTALL_DIR}   )

