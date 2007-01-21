macro_bool_to_01(X11VIDMODE_FOUND HAVE_VIDMODE_EXTENSION)

configure_file (config-krdc.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-krdc.h )

add_subdirectory( vnc ) 
add_subdirectory( rdp ) 




########### next target ###############

include_directories(
${CMAKE_BINARY_DIR}/krdc
${CMAKE_BINARY_DIR}/krdc/vnc
${CMAKE_BINARY_DIR}/krdc/rdp
)

set(krdc_SRCS 
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

kde4_add_ui_files(krdc_SRCS hostprofiles.ui maindialogbase.ui )
kde4_add_ui3_files(krdc_SRCS keycapturewidget.ui )

kde4_add_executable(krdc ${krdc_SRCS})

target_link_libraries(krdc  ${KDE4_KDEUI_LIBS} vnc rdp ${KDE4_KDNSSD_LIBS} kwalletclient ${SLP_LIBRARIES})
if (HAVE_VIDMODE_EXTENSION)
  target_link_libraries(krdc ${X11VIDMODE_LIBRARIES})
endif (HAVE_VIDMODE_EXTENSION)

install(TARGETS krdc  DESTINATION ${BIN_INSTALL_DIR} )


########### install files ###############

install( FILES krdc.desktop  DESTINATION ${XDG_APPS_DIR})
install( FILES smb2rdc.desktop  DESTINATION ${DATA_INSTALL_DIR}/konqueror/servicemenus)
install( FILES pointcursor.png pointcursormask.png pinup.png pindown.png    iconify.png close.png  DESTINATION ${DATA_INSTALL_DIR}/krdc/pics)
install( FILES vnc.protocol rdp.protocol  DESTINATION ${SERVICES_INSTALL_DIR})
install( FILES _rfb._tcp  DESTINATION ${DATA_INSTALL_DIR}/zeroconf)

kde4_install_icons( ${ICON_INSTALL_DIR}   )



