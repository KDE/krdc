KDE_CXXFLAGS = $(USE_THREADS)

SUBDIRS = vnc

METASOURCES = AUTO

bin_PROGRAMS = krdc
krdc_SOURCES = main.cpp krdc.cpp vidmode.cpp kfullscreenpanel.cpp \
  newconnectiondialog.ui fullscreentoolbar.ui toolbar.ui \
  kservicelocator.cpp srvlocncdialog.cpp keycapturedialog.ui \
  keycapturedialog2.cpp rdpconnectingdialog.ui

noinst_HEADERS = main.h krdc.h vidmode.h kfullscreenpanel.h events.h \
   newconnectiondialog.ui.h kservicelocator.h srvlocncdialog.h \
   keycapturedialog2.h

EXTRA_DIST = README TODO window_fullscreen.png window_nofullscreen.png \
             iconify.png close.png scale.png

datadir = $(kde_datadir)/krdc/pics
data_DATA =  pointcursor.png pointcursormask.png pinup.png pindown.png 


krdc_LDADD   = vnc/libvnc.la $(LIB_KDEUI) $(LIBXF86VIDMODE) $(LIB_SLP)
krdc_LDFLAGS = $(all_libraries) $(KDE_RPATH)

lnkdir = $(kde_appsdir)/Internet
lnk_DATA = krdc.desktop

kde_services_DATA = vnc.protocol

KDE_ICON = krdc

INCLUDES= $(all_includes)

messages: rc.cpp
	$(XGETTEXT) *.cpp *.h -o $(podir)/krdc.pot
