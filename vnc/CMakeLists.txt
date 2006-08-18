
include_directories( ${CMAKE_SOURCE_DIR}/krdc  )


########### next target ###############
set(krdc_vnc_SRCS
    kvncview.cpp threads.cpp colour.c d3des.c desktop.c rfbproto.c sockets.c vncauth.c vnchostpref.cpp vncprefs.cpp
)

kde4_automoc(${krdc_vnc_SRCS})

kde4_add_ui_files(krdc_vnc_SRCS vncprefs.ui )

kde4_add_library(vnc STATIC ${krdc_vnc_SRCS})

target_link_libraries( vnc ${KDE4_KDECORE_LIBS} ${QT_QTGUI_LIBRARY} ${JPEG_LIBRARIES} ${X11VIDMODE_LIBRARIES} )


########### install files ###############





#original Makefile.am contents follow:

#KDE_CXXFLAGS = $(USE_THREADS)
#
#METASOURCES = AUTO
#
#noinst_LTLIBRARIES = libvnc.la
#
#libvnc_la_SOURCES = kvncview.cpp threads.cpp colour.c d3des.c desktop.c \
#  rfbproto.c sockets.c vncauth.c vncprefs.ui vnchostpref.cpp
#
#noinst_HEADERS = kvncview.h rfbproto.h  vncviewer.h vnctypes.h vncauth.h \
#   pointerlatencyometer.h threads.h d3des.h vnchostpref.h
#
#libvnc_la_LIBADD   = $(LIB_KDEUI) $(LIBJPEG)
#libvnc_la_LDFLAGS = $(all_libraries) $(KDE_RPATH)
#
#INCLUDES= -I$(top_srcdir)/krdc -I.. $(all_includes)
