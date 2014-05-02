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

pkglibexec_SCRIPTS = \
	$(common_scripts) \
	$(NULL)

nobase_pkglibexec_SCRIPTS = \
	$(install_scripts) \
	$(uninstall_scripts) \
	$(NULL)

EXTRA_DIST = \
	$(common_scripts) \
        $(install_scripts) \
	$(uninstall_scripts) \
	$(NULL)
