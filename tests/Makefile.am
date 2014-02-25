include $(top_srcdir)/glib-tap.mk

AM_CFLAGS = -g $(DEPENDENCIES_CFLAGS)
LDADD = $(top_builddir)/src/libeam-1.0.la $(DEPENDENCIES_LIBS)

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Eam\" \
	-I$(top_srcdir)/src \
	$(NULL)

dist_test_data = \
	appdir/app01/.info \
	$(NULL)

test_programs = \
	pkg \
	version \
	$(NULL)
