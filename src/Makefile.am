bin_PROGRAMS = eamd

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Eam\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-DLOCALSTATEDIR=\"$(localstatedir)\" \
	-DPKGLIBEXECDIR=\"$(pkglibexecdir)\" \
	-DPKGDATADIR=\"$(pkgdatadir)\" \
	$(NULL)

source_c = \
	eam-dbus-server.c \
	eam-dbus-utils.c \
	eam-service.c \
	eam-config.c \
	eam-transaction.c \
	eam-transaction-dbus.c \
	eam-install.c \
	eam-update.c \
	eam-uninstall.c \
	eam-fs-utils.c \
	eam-log.c \
	eam-error.c \
	eam-utils.c \
	$(NULL)

source_h = \
	eam-dbus-server.h \
	eam-dbus-utils.h \
	eam-service.h \
	eam-config.h \
	eam-transaction.h \
	eam-transaction-dbus.h \
	eam-install.h \
	eam-update.h \
	eam-uninstall.h \
	eam-fs-utils.h \
	eam-log.h \
	eam-error.h \
	eam-utils.h \
	$(NULL)

eam_built_sources = \
	eam-resources.h \
	eam-resources.c \
	eam-service-generated.h \
	eam-service-generated.c \
	$(NULL)

gresource_file = $(top_srcdir)/data/eam.gresource.xml
resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir=$(top_srcdir)/data $(gresource_file))
eam-resources.c: $(gresource_file) $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) \
		--sourcedir=$(top_builddir)/data \
		--target=$@ \
		--generate-source \
	$<
eam-resources.h: $(gresource_file) $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) \
		--sourcedir=$(top_builddir)/data \
		--target=$@ \
		--generate-header \
	$<

eam-service-generated.h eam-service-generated.c: $(top_srcdir)/data/eam-service-interface.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--interface-prefix com.endlessm. \
		--generate-c-code eam-service-generated \
		--c-namespace Eam \
		$<

BUILT_SOURCES = $(eam_built_sources)
CLEANFILES = $(eam_built_sources)

noinst_LTLIBRARIES = libeam.la
libeam_la_CFLAGS = $(EAM_CFLAGS)
libeam_la_LIBADD = $(EAM_LIBS)
libeam_la_SOURCES = $(source_c) $(source_h) $(BUILT_SOURCES)
nodist_libeam_la_SOURCES = $(eam_built_sources) 

eamd_CPPFLAGS = -DLOCALEDIR=\"$(datadir)/locale\" $(AM_CPPFLAGS)
eamd_CFLAGS = $(EAM_CFLAGS)
eamd_LDADD = $(EAM_LIBS) libeam.la
eamd_LDFLAGS = -export-dynamic
eamd_SOURCES = main.c
