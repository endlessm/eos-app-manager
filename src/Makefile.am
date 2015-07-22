bin_PROGRAMS =
lib_LTLIBRARIES =

EXTRA_DIST =
CLEANFILES =

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Eam\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-DLOCALSTATEDIR=\"$(localstatedir)\" \
	-DPKGLIBEXECDIR=\"$(pkglibexecdir)\" \
	-DPKGDATADIR=\"$(pkgdatadir)\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DEAM_COMPILATION

public_source_h = \
	eam-config.h \
	eam-error.h \
	eam-fs.h \
	eam-install.h \
	eam-transaction.h \
	eam-uninstall.h \
	eam-update.h \
	eam-utils.h

lib_sources = \
	eam-config.c \
	eam-error.c \
	eam-fs.c \
	eam-install.c \
	eam-log-private.h eam-log.c \
	eam-transaction.c \
	eam-uninstall.c \
	eam-update.c \
	eam-utils.c

daemon_sources = \
	eam-dbus-server.h eam-dbus-server.c \
	eam-dbus-utils.h eam-dbus-utils.c \
	eam-service.h eam-service.c

eam_built_sources = \
	eam-resources.h eam-resources.c \
	$(NULL)

gresource_file = $(top_srcdir)/data/eam.gresource.xml
resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir=$(top_srcdir)/data $(gresource_file))
eam-resources.c: $(gresource_file) $(resource_files) eam-resources.h
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
CLEANFILES += $(eam_built_sources)

lib_LTLIBRARIES += libeam.la
libeam_la_CFLAGS = $(EAM_CFLAGS)
libeam_la_LIBADD = $(EAM_LIBS)
libeam_la_SOURCES = $(lib_sources)

eam_includedir = $(includedir)/eos-app-manager
eam_include_HEADERS = $(public_source_h) eam.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = eam.pc
EXTRA_DIST += eam.pc.in

bin_PROGRAMS += eamd
eamd_CFLAGS = $(EAM_CFLAGS)
eamd_LDADD = $(EAM_LIBS) libeam.la
eamd_LDFLAGS = -export-dynamic
eamd_SOURCES = $(daemon_sources) main.c
nodist_eamd_SOURCES = $(eam_built_sources) 
