AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Eam\"

noinst_PROGRAMS = wclient
wclient_CFLAGS = $(DEPENDENCIES_CFLAGS) -I$(top_srcdir)/src
wclient_LIBS = $(DEPENDENCIES_LIBS)
wclient_LDADD = $(top_builddir)/src/libeam-1.0.la
wclient_SOURCE = wclient.c

do_subst = sed \
	-e 's|@DATA_DIR[@]|$(datadir)|g' \
	-e 's|@SYSCONF_DIR[@]|$(sysconfdir)|g'

eos-select-app-server: eos-select-app-server.in Makefile
	$(AM_V_GEN) $(do_subst) $< > $@
	chmod +x $@

EXTRA_DIST = \
	dbus-client.sh \
	eos-select-app-server.in \
	$(NULL);

bin_SCRIPTS = \
	eos-select-app-server \
	$(NULL)

-include $(top_srcdir)/git.mk
