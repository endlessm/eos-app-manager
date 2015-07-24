bin_PROGRAMS =
bin_SCRIPTS =

BUILT_SOURCES =

CLEANFILES =
DISTCLEANFILES =

EXTRA_DIST =

completiondir = $(datadir)/bash-completion/completions
completion_DATA = completion/eamctl
EXTRA_DIST += $(completion_DATA)

do_subst = sed \
	-e 's|@DATA_DIR[@]|$(datadir)|g' \
	-e 's|@SYSCONF_DIR[@]|$(sysconfdir)|g'

eos-select-app-server: eos-select-app-server.in Makefile
	$(AM_V_GEN) $(do_subst) $< > $@
	chmod +x $@

bin_SCRIPTS += eos-select-app-server
EXTRA_DIST += eos-select-app-server.in
CLEANFILES += eos-select-app-server

bin_PROGRAMS += eamctl

eos-app-manager-transaction.h: Makefile $(top_srcdir)/data/eam-transaction-interface.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--interface-prefix com.endlessm. \
		--generate-c-code eos-app-manager-transaction \
		--c-namespace Eos \
		$(top_srcdir)/data/eam-transaction-interface.xml

eos-app-manager-transaction.c: eos-app-manager-transaction.h

BUILT_SOURCES += eos-app-manager-transaction.h eos-app-manager-transaction.c
DISTCLEANFILES += eos-app-manager-transaction.h eos-app-manager-transaction.c

eos-app-manager-service.h: Makefile $(top_srcdir)/data/eam-service-interface.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--interface-prefix com.endlessm. \
		--generate-c-code eos-app-manager-service \
		--c-generate-object-manager \
		--c-namespace Eos \
		$(top_srcdir)/data/eam-service-interface.xml

eos-app-manager-service.c: eos-app-manager-service.h

BUILT_SOURCES += eos-app-manager-service.h eos-app-manager-service.c
DISTCLEANFILES += eos-app-manager-service.h eos-app-manager-service.c

eamctl_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"eamctl\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/src \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/tools \
	-I$(top_builddir)/tools
eamctl_CFLAGS = $(EAM_CFLAGS)
eamctl_LDADD = $(EAM_LIBS) $(top_builddir)/src/libeam.la
eamctl_LDFLAGS = -export-dynamic
eamctl_SOURCES = \
	$(BUILT_SOURCES) \
	eam-command-app-info.c \
	eam-command-config.c \
	eam-command-create-symlinks.c \
	eam-command-ensure-symlink-farm.c \
	eam-command-help.c \
	eam-command-init-fs.c \
	eam-command-install.c \
	eam-command-list-apps.c \
	eam-command-migrate.c \
	eam-command-uninstall.c \
	eam-command-update.c \
	eam-command-version.c \
	eam-commands.c \
	eam-commands.h \
	eam.c
