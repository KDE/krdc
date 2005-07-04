KDE_CXXFLAGS = $(USE_THREADS)

SUBDIRS = vnc rdp

METASOURCES = AUTO

bin_PROGRAMS = krdc
krdc_SOURCES = main.cpp krdc.cpp vidmode.cpp kfullscreenpanel.cpp \
  hostprofiles.ui maindialogbase.ui keycapturewidget.ui kservicelocator.cpp \
  keycapturedialog.cpp kremoteview.cpp smartptr.cpp hostpreferences.cpp \
  preferencesdialog.cpp maindialogwidget.cpp maindialog.cpp

noinst_HEADERS = main.h krdc.h vidmode.h kfullscreenpanel.h events.h \
  kservicelocator.h preferencesdialog.h \
  keycapturedialog.h kremoteview.h smartptr.h hostpreferences.h

EXTRA_DIST = README TODO

appdatadir = $(kde_datadir)/krdc/pics
appdata_DATA =  pointcursor.png pointcursormask.png pinup.png pindown.png \
  iconify.png close.png

dnssddatadir = $(kde_datadir)/zeroconf
dnssddata_DATA = _rfb._tcp

krdc_LDADD   = vnc/libvnc.la rdp/librdp.la $(LIB_KDEUI) $(LIBXF86VIDMODE) $(LIB_SLP) $(LIB_KDNSSD) $(X_LDFLAGS) $(LIB_X11) -lkwalletclient
krdc_LDFLAGS = $(all_libraries) $(KDE_RPATH)

xdg_apps_DATA = krdc.desktop

servicedir = $(kde_datadir)/konqueror/servicemenus
service_DATA = smb2rdc.desktop

kde_services_DATA = vnc.protocol rdp.protocol

KDE_ICON = krdc

INCLUDES= $(all_includes)

messages: rc.cpp
	$(EXTRACTRC) *.ui */*.ui >> rc.cpp
	$(XGETTEXT) *.cpp */*.cpp *.h -o $(podir)/krdc.pot
