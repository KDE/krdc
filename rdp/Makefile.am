# kdelibs/kio/kssl/Makefile.am says this about OpenSSL headers:
# don't change the order of this one. it breaks when openssl is in /usr/include
# and the distro's KDE is there too, but the current "devel" KDE prefix is
# somewhere else. Another option for the future (needs testing) would be to
# change SSL_INCLUDES to .../include/openssl and make the source use #include <f
# instead of #include <openssl/foo.h>
INCLUDES=-I$(top_srcdir)/krdc $(all_includes) $(SSL_INCLUDES)

KDE_CXXFLAGS = $(USE_THREADS)

METASOURCES = AUTO

noinst_LTLIBRARIES = librdp.la

librdp_la_SOURCES = bitmap.c cache.c ewmhints.c iso.c krdpview.cpp licence.c \
                    mcs.c orders.c rdesktop.c rdp.c secure.c tcp.c \
                    threads.cpp xkeymap.c xwin.c

noinst_HEADERS = krdpview.h constants.h krdpview.h orders.h parse.h \
                 proto.h rdesktop.h scancodes.h threads.h types.h
librdp_la_LIBADD   = $(LIB_KDEUI) $(LIBJPEG) $(LIBSSL) 
librdp_la_LDFLAGS = $(all_libraries) $(SSL_LDFLAGS) $(KDE_RPATH)

keymapdir = $(kde_datadir)/krdc/rdp-keymaps

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(keymapdir)
	for file in $(srcdir)/keymaps/*; do \
		if test -f $$file; then \
		   $(INSTALL_DATA) $$file $(DESTDIR)$(keymapdir)/`echo $$file|sed "s,^.*/,,"`;\
		fi \
	done

uninstall-local:
	rm -r $(DESTDIR)$(kde_datadir)/krdc/rdp-keymaps/$$file

AM_CFLAGS = -DWITH_OPENSSL -DKEYMAP_PATH=\"$(keymapdir)\"

messages: rc.cpp
	$(XGETTEXT) *.cpp *.h -o $(podir)/krdc.pot

