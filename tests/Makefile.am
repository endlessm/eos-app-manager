include $(top_srcdir)/build/autotools/glib-tap.mk

TEST_ENVIRONMENT = EAM_TESTING=1

AM_CFLAGS = -g $(DEPENDENCIES_CFLAGS)
LDADD = $(top_builddir)/src/libeam.la $(DEPENDENCIES_LIBS)

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Eam\" \
	-I$(top_srcdir)/src \
	$(NULL)

dist_test_data = \
	$(NULL)

test_programs = \
	$(NULL)
