INSTALL_SCRIPT = ${INSTALL} -m 750

common_scripts = \
	utils.sh \
	install-config.sh \
	$(NULL)

install_scripts = \
	install/01-extract.sh \
	install/02-symlinks.sh \
	install/03-desktop-updates.sh \
	install/04-cleanup.sh \
	install_rollback/01-delete-symlinks.sh \
	install_rollback/02-cleanup.sh \
	install_rollback/03-desktop-updates.sh \
	$(NULL)

uninstall_scripts = \
	uninstall/01-existence-check.sh \
	uninstall/02-delete-symlinks.sh \
	uninstall/03-delete-installation-dir.sh \
	uninstall/04-desktop-updates.sh \
	$(NULL)

update_scripts = \
	update/full_update/01-extract.sh \
	update/full_update/02-delete-symlinks.sh \
	update/full_update/03-move-out-installation-dir.sh \
	update/full_update/04-move-update-to-prefix.sh \
	update/full_update/05-symlinks.sh \
	update/full_update/06-desktop-updates.sh \
	update/full_update/07-cleanup.sh \
	update/rollback/01-delete-symlinks.sh \
	update/rollback/02-move-back-installation-dir.sh \
	update/rollback/03-regenerate-symlinks.sh \
	update/rollback/04-desktop-updates.sh \
	update/rollback/05-cleanup-downloads.sh \
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
