KDE_CXXFLAGS = $(USE_THREADS)

METASOURCES = AUTO

bin_PROGRAMS = krdc
krdc_SOURCES = main.cpp krdc.cpp kvncview.cpp threads.cpp vidmode.cpp \
  kfullscreenpanel.cpp colour.c d3des.c desktop.c rfbproto.c sockets.c \
  vncauth.c newconnectiondialog.ui passworddialog.ui fullscreentoolbar.ui \
  toolbar.ui

noinst_HEADERS = main.h krdc.h kvncview.h vidmode.h rfbproto.h  \
   kfullscreenpanel.h vncviewer.h vnctypes.h vncauth.h events.h \
   threads.h newconnectiondialog.ui.h 

EXTRA_DIST = README TODO

datadir = $(kde_datadir)/krdc/pics
data_DATA = window_fullscreen.png window_nofullscreen.png \
            pinup.png pindown.png iconify.png close.png scale.png

krdc_LDADD   = $(LIB_KDEUI) $(LIBJPEG) $(LIBXF86VIDMODE)
krdc_LDFLAGS = $(all_libraries) $(KDE_RPATH)

lnkdir = $(kde_appsdir)/Internet
lnk_DATA = krdc.desktop

kde_services_DATA = vnc.protocol

INCLUDES= $(all_includes)

messages: rc.cpp
	$(XGETTEXT) rc.cpp *.cpp *.cc -o $(podir)/krdc.pot
