include $(top_srcdir)/glib-tap.mk

TEST_ENVIRONMENT = EAM_TESTING=1

AM_CFLAGS = -g $(DEPENDENCIES_CFLAGS)
LDADD = $(top_builddir)/src/libeam.la $(DEPENDENCIES_LIBS)

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Eam\" \
	-I$(top_srcdir)/src \
	$(NULL)

dist_test_data = \
	appdir/app01/.info \
	appdir/com.application.id1/.info \
	appdir/com.application.id2/.info \
	data/link-db.json \
	data/update-db.json \
	data/available.json \
	data/test-app.tar.gz \
	data/test-app.sha256 \
	data/test-app.asc \
	data/dummy.pub \
	scriptdir/README \
	scriptdir/script3 \
	scriptdir/script2 \
	scriptdir/script1 \
	scriptdir/.hidden \
	$(NULL)

test_programs = \
	pkg \
	version \
	cfg \
	rest \
	updates \
	spawner \
	scripts \
	$(NULL)

-include $(top_srcdir)/git.mk
