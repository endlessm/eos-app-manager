bin_PROGRAMS = test

AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Eam\"

source_c = \
	eam-pkg.c \
	eam-pkgdb.c \
	$(NULL)

source_h = \
	eam-pkg.h \
	eam-pkgdb.h \
	$(NULL)

test_CFLAGS = $(DEPENDENCIES_CFLAGS)
test_LDADD = $(DEPENDENCIES_LIBS)
test_SOURCES = $(source_c) $(source_h) test.c

noinst_LTLIBRARIES = libeam-1.0.la
libeam_1_0_la_CFLAGS = $(DEPENDENCIES_CFLAGS)
libeam_1_0_la_LIBADD = $(DEPENDENCIES_LIBS)
libeam_1_0_la_SOURCES = $(source_c) $(source_h)
