# Process this file with automake to produce Makefile.in

LTVERSIONINFO = $(INTERFACE_NUM):$(IMPLEMENTATION_NUM):$(INTERFACES_SUPPORTED)

AM_CPPFLAGS = \
	"-DPACKAGE_LOCALE_DIR=\"$(localedir)\"" \
	"-DPACKAGE_SRC_DIR=\"$(srcdir)\"" \
	"-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\"" \
	"-DG_LOG_DOMAIN=\"GNUnetWorker\""

AM_CFLAGS = \
	-Wall\
	-g \
	-pedantic \
	$(WARN_CFLAGS) \
	$(GNUNET_CFLAGS) \
	$(GNUNET_WORKER_CFLAGS)

lib_LTLIBRARIES = \
	lib@PROJECT_NAME@.la

lib@PROJECT_NAME@_la_SOURCES = \
	gnunet_worker_lib.h \
	worker.c \
	worker.h

lib@PROJECT_NAME@_la_LDFLAGS = \
	-version-info $(LTVERSIONINFO)

lib@PROJECT_NAME@_la_LIBADD = \
	$(GNUNET_WORKER_LIBS)

otherincludedir = $(includedir)/gnunet

otherinclude_HEADERS = \
	gnunet_worker_lib.h

pkgconfig_DATA = \
	@PROJECT_NAME@.pc

EXTRA_DIST = \
	@PROJECT_NAME@.pc.in


# EOF

