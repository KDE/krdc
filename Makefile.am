KDE_CXXFLAGS = $(USE_THREADS)

METASOURCES = AUTO

bin_PROGRAMS = krdc
krdc_SOURCES = main.cpp krdc.cpp kvncview.cpp threads.cpp vidmode.cpp \
  kfullscreenpanel.cpp colour.c d3des.c desktop.c rfbproto.c sockets.c \
  vncauth.c newconnectiondialog.ui passworddialog.ui fullscreentoolbar.ui \
  toolbar.ui kservicelocator.cpp srvlocncdialog.cpp keycapturedialog.ui \
  keycapturedialog2.cpp

noinst_HEADERS = main.h krdc.h kvncview.h vidmode.h rfbproto.h  \
   kfullscreenpanel.h vncviewer.h vnctypes.h vncauth.h events.h \
   pointerlatencyometer.h threads.h newconnectiondialog.ui.h  \
   kservicelocator.h srvlocncdialog.h threadsafeeventreceiver.h \
   keycapturedialog2.h

EXTRA_DIST = README TODO window_fullscreen.png window_nofullscreen.png \
             iconify.png close.png scale.png

datadir = $(kde_datadir)/krdc/pics
data_DATA =  pointcursor.png pointcursormask.png pinup.png pindown.png 


krdc_LDADD   = $(LIB_KDEUI) $(LIBJPEG) $(LIBXF86VIDMODE) $(LIB_SLP)
krdc_LDFLAGS = $(all_libraries) $(KDE_RPATH)

lnkdir = $(kde_appsdir)/Internet
lnk_DATA = krdc.desktop

kde_services_DATA = vnc.protocol

KDE_ICON = krdc

INCLUDES= $(all_includes)

messages: rc.cpp
	$(XGETTEXT) *.cpp *.h -o $(podir)/krdc.pot
