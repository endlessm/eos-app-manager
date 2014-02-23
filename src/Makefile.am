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
