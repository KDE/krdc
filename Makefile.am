KDE_CXXFLAGS = $(USE_THREADS) -DWITH_OPENSSL

SUBDIRS = vnc rdp

METASOURCES = AUTO

bin_PROGRAMS = krdc
krdc_SOURCES = main.cpp krdc.cpp vidmode.cpp kfullscreenpanel.cpp \
  maindialog.ui fullscreentoolbar.ui toolbar.ui \
  kservicelocator.cpp srvlocmaindialog.cpp keycapturedialog.ui \
  keycapturedialog2.cpp kremoteview.cpp

noinst_HEADERS = main.h krdc.h vidmode.h kfullscreenpanel.h events.h \
   maindialog.ui.h kservicelocator.h srvlocmaindialog.h \
   keycapturedialog2.h kremoteview.h

EXTRA_DIST = README TODO window_fullscreen.png window_nofullscreen.png \
             iconify.png close.png scale.png

appdatadir = $(kde_datadir)/krdc/pics
appdata_DATA =  pointcursor.png pointcursormask.png pinup.png pindown.png 


krdc_LDADD   = vnc/libvnc.la rdp/librdp.la $(LIB_KDEUI) $(LIBXF86VIDMODE) $(LIB_SLP) -L/usr/X11R6/lib -lX11 $(LIBSSL)
krdc_LDFLAGS = $(all_libraries) $(KDE_RPATH) $(SSL_LDFLAGS) 

xdg_apps_DATA = krdc.desktop

kde_services_DATA = vnc.protocol rdp.protocol

KDE_ICON = krdc

INCLUDES= $(all_includes)

messages: rc.cpp
	$(XGETTEXT) *.cpp *.h -o $(podir)/krdc.pot
