bin_PROGRAMS = eam

AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Eam\"

source_c = \
	eam-pkg.c \
	eam-pkgdb.c \
	eam-version.c \
	$(NULL)

source_h = \
	eam-pkg.h \
	eam-pkgdb.h \
	eam-version.h \
	$(NULL)

eam_CFLAGS = $(DEPENDENCIES_CFLAGS)
eam_LDADD = $(DEPENDENCIES_LIBS)
eam_SOURCES = $(source_c) $(source_h) main.c

noinst_LTLIBRARIES = libeam-1.0.la
libeam_1_0_la_CFLAGS = $(DEPENDENCIES_CFLAGS)
libeam_1_0_la_LIBADD = $(DEPENDENCIES_LIBS)
libeam_1_0_la_SOURCES = $(source_c) $(source_h)
