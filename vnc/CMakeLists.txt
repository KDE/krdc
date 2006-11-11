
include_directories( ${CMAKE_SOURCE_DIR}/krdc  ${JPEG_INCLUDE_DIR} )


########### next target ###############
set(krdc_vnc_SRCS
    kvncview.cpp threads.cpp colour.c d3des.c desktop.c rfbproto.c sockets.c vncauth.c vnchostpref.cpp vncprefs.cpp
)

kde4_automoc(${krdc_vnc_SRCS})

kde4_add_ui_files(krdc_vnc_SRCS vncprefs.ui )

kde4_add_library(vnc STATIC ${krdc_vnc_SRCS})

target_link_libraries( vnc ${KDE4_KDECORE_LIBS} ${QT_QTGUI_LIBRARY} ${JPEG_LIBRARIES} )


########### install files ###############





