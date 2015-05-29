bin_PROGRAMS = eam

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Eam\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	$(NULL)

source_c = \
	eam-dbus-server.c \
	eam-dbus-utils.c \
	eam-service.c \
	eam-config.c \
	eam-spawner.c \
	eam-transaction.c \
	eam-install.c \
	eam-update.c \
	eam-uninstall.c \
	eam-fs-sanity.c \
	eam-log.c \
	eam-error.c \
	eam-utils.c \
	$(NULL)

source_h = \
	eam-dbus-server.h \
	eam-dbus-utils.h \
	eam-service.h \
	eam-config.h \
	eam-spawner.h \
	eam-transaction.h \
	eam-install.h \
	eam-update.h \
	eam-uninstall.h \
	eam-fs-sanity.h \
	eam-log.h \
	eam-error.h \
	eam-utils.h \
	$(NULL)

eam_built_sources = \
	eam-resources.h \
	eam-resources.c \
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

BUILT_SOURCES = $(eam_built_sources)

noinst_LTLIBRARIES = libeam.la
libeam_la_CFLAGS = $(EAM_CFLAGS)
libeam_la_LIBADD = $(EAM_LIBS)
libeam_la_SOURCES = $(source_c) $(source_h) $(BUILT_SOURCES)
nodist_libeam_la_SOURCES = $(eam_built_sources) 

eam_CPPFLAGS = -DLOCALEDIR=\"$(datadir)/locale\" $(AM_CPPFLAGS)
eam_CFLAGS = $(EAM_CFLAGS)
eam_LDADD = $(EAM_LIBS) libeam.la
eam_LDFLAGS = -export-dynamic
eam_SOURCES = main.c
