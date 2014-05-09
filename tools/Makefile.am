AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Eam\"

noinst_PROGRAMS = wclient
wclient_CFLAGS = $(DEPENDENCIES_CFLAGS) -I$(top_srcdir)/src
wclient_LIBS = $(DEPENDENCIES_LIBS)
wclient_LDADD = $(top_builddir)/src/libeam-1.0.la
wclient_SOURCE = wclient.c

EXTRA_DIST = dbus-client.sh deb2bundle.py

-include $(top_srcdir)/git.mk
