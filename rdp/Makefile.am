INCLUDES=-I$(top_srcdir)/krdc $(all_includes)

METASOURCES = AUTO

noinst_LTLIBRARIES = librdp.la

librdp_la_SOURCES = krdpview.cpp rdphostpref.cpp rdpprefs.ui

noinst_HEADERS = krdpview.h rdphostpref.h
librdp_la_LIBADD   = $(LIB_KDEUI) $(LIBJPEG)
librdp_la_LDFLAGS = $(all_libraries) $(KDE_RPATH)
