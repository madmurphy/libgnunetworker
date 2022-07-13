# Process this file with automake to produce Makefile.in

LTVERSIONINFO = $(INTERFACE_NUM):$(IMPLEMENTATION_NUM):$(INTERFACES_SUPPORTED)

AM_CPPFLAGS = \
	'-DPACKAGE_LOCALE_DIR="$(localedir)"' \
	'-DPACKAGE_SRC_DIR="$(srcdir)"' \
	'-DPACKAGE_DATA_DIR="$(pkgdatadir)"'

AM_CFLAGS = \
	-Wall\
	-pedantic \
	-g \
	-O2 \
	-std=c11 \
	$(GNUNET_CFLAGS) \
	$(WARN_CFLAGS)

gnunetincludedir = $(includedir)/gnunet

gnunetinclude_HEADERS = \
	include/gnunet_worker_lib.h

lib_LTLIBRARIES = \
	lib@PROJECT_NAME@.la

lib@PROJECT_NAME@_la_SOURCES = \
	requirement.h \
	worker.c \
	worker.h

lib@PROJECT_NAME@_la_LDFLAGS = \
	-version-info $(LTVERSIONINFO)

lib@PROJECT_NAME@_la_LIBADD = \
	$(PTHREAD_LIBS) \
	$(GNUNET_WORKER_LIBS)

lib@PROJECT_NAME@_la_CFLAGS = \
	$(PTHREAD_CFLAGS) \
	$(GNUNET_WORKER_CFLAGS)

pkgconfig_DATA = \
	@PROJECT_NAME@.pc

EXTRA_DIST = \
	@PROJECT_NAME@.pc.in


# EOF

