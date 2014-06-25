INSTALL_SCRIPT = ${INSTALL} -m 750

common_scripts = \
	utils.sh \
	config.sh \
	$(NULL)

install_scripts = \
	install/run/10-extract.sh \
	install/run/20-external.sh\
	install/run/30-install.sh \
	install/run/40-symlinks.sh \
	install/run/50-desktop-updates.sh \
	install/run/60-cleanup.sh \
	install/rollback/01-delete-symlinks.sh \
	install/rollback/02-cleanup.sh \
	install/rollback/03-desktop-updates.sh \
	$(NULL)

uninstall_scripts = \
	uninstall/01-delete-symlinks.sh \
	uninstall/02-delete-installation-dir.sh \
	uninstall/03-desktop-updates.sh \
	$(NULL)

update_scripts = \
	update/full/01-extract.sh \
	update/full/02-delete-symlinks.sh \
	update/full/03-move-out-installation-dir.sh \
	update/full/04-move-update-to-prefix.sh \
	update/full/05-symlinks.sh \
	update/full/06-desktop-updates.sh \
	update/full/07-cleanup.sh \
	update/rollback/01-delete-symlinks.sh \
	update/rollback/02-move-back-installation-dir.sh \
	update/rollback/03-symlinks.sh \
	update/rollback/04-desktop-updates.sh \
	update/rollback/05-cleanup.sh \
	$(NULL)

pkglibexec_SCRIPTS = \
	$(common_scripts) \
	$(NULL)

nobase_pkglibexec_SCRIPTS = \
	$(install_scripts) \
	$(uninstall_scripts) \
	$(update_scripts) \
	$(NULL)

EXTRA_DIST = \
	$(common_scripts) \
        $(install_scripts) \
	$(uninstall_scripts) \
	$(update_scripts) \
	$(NULL)

-include $(top_srcdir)/git.mk
