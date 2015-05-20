do_subst = sed \
	-e 's|@DATA_DIR[@]|$(datadir)|g' \
	-e 's|@SYSCONF_DIR[@]|$(sysconfdir)|g'

eos-select-app-server: eos-select-app-server.in Makefile
	$(AM_V_GEN) $(do_subst) $< > $@
	chmod +x $@

EXTRA_DIST = \
	eos-select-app-server.in \
	bundler-tool.sh \
	$(NULL)

CLEANFILES = \
	eos-select-app-server \
	$(NULL)

bin_SCRIPTS = \
	eos-select-app-server \
	$(NULL)

-include $(top_srcdir)/build/autotools/git.mk
