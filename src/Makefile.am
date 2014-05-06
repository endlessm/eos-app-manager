bin_PROGRAMS = eam

AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Eam\"

source_c = \
	eam-pkg.c \
	eam-pkgdb.c \
	eam-version.c \
	eam-dbus-server.c \
	eam-resources.c \
	eam-service.c \
	eam-config.c \
	eam-wc.c \
	eam-wc-file.c \
	eam-os.c \
	eam-rest.c \
	eam-updates.c \
	eam-refresh.c \
	eam-spawner.c \
	eam-transaction.c \
	eam-install.c \
	eam-fs-sanity.c \
	eam-list-avail.c \
	$(NULL)

source_h = \
	eam-pkg.h \
	eam-pkgdb.h \
	eam-version.h \
	eam-dbus-server.h \
	eam-service.h \
	eam-config.h \
	eam-wc.h \
	eam-wc-file.h \
	eam-os.h \
	eam-rest.h \
	eam-updates.h \
	eam-refresh.h \
	eam-spawner.h \
	eam-transaction.h \
	eam-install.h \
	eam-fs-sanity.h \
	eam-list-avail.h \
	$(NULL)

BUILT_SOURCES = \
	eam-resources.c \
	$(NULL)

eam_CPPFLAGS = -DLOCALEDIR=\"$(datadir)/locale\" $(AM_CPPFLAGS)
eam_CFLAGS = $(DEPENDENCIES_CFLAGS)
eam_LDADD = $(DEPENDENCIES_LIBS)
eam_SOURCES = $(source_c) $(source_h) main.c

noinst_LTLIBRARIES = libeam-1.0.la
libeam_1_0_la_CFLAGS = $(DEPENDENCIES_CFLAGS)
libeam_1_0_la_LIBADD = $(DEPENDENCIES_LIBS)
libeam_1_0_la_SOURCES = $(source_c) $(source_h)

gresource_file = $(top_srcdir)/data/eam.gresource.xml
resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir=$(top_srcdir)/data $(gresource_file))
eam-resources.c: $(gresource_file) $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) \
		--sourcedir=$(top_builddir)/data \
		--target=$@ \
		--generate-source \
		$<
