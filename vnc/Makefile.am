KDE_CXXFLAGS = $(USE_THREADS)

METASOURCES = AUTO

noinst_LTLIBRARIES = libvnc.la

libvnc_la_SOURCES = kvncview.cpp threads.cpp colour.c d3des.c desktop.c \
  rfbproto.c sockets.c vncauth.c passworddialog.ui

noinst_HEADERS = kvncview.h rfbproto.h  vncviewer.h vnctypes.h vncauth.h \
   pointerlatencyometer.h threads.h threadsafeeventreceiver.h d3des.h

libvnc_la_LIBADD   = $(LIB_KDEUI) $(LIBJPEG) 
libvnc_la_LDFLAGS = $(all_libraries) $(KDE_RPATH)

INCLUDES= $(all_includes) -I$(top_srcdir)/krdc

messages: rc.cpp
	$(XGETTEXT) *.cpp *.h -o $(podir)/krdc.pot
