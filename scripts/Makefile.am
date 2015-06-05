common_scripts = \
	utils.sh \
	config.sh \
	$(NULL)

update_scripts = \
	update/full/05-directories.sh \
	update/full/10-extract.sh \
	update/full/15-external.sh \
	update/full/20-custom-installer.sh \
	update/full/25-cleanup-external.sh \
	update/full/30-delete-symlinks.sh \
	update/full/35-move-out-installation-dir.sh \
	update/full/40-move-update-to-prefix.sh \
	update/full/45-symlinks.sh \
	update/full/50-desktop-updates.sh \
	update/full/50-python-compile.sh \
	update/full/55-cleanup.sh \
	update/xdelta/05-directories.sh \
	update/xdelta/10-verify-downloads.sh \
	update/xdelta/15-python-cleanup.sh \
	update/xdelta/20-apply-xdelta.sh \
	update/xdelta/30-delete-symlinks.sh \
	update/xdelta/35-move-out-installation-dir.sh \
	update/xdelta/40-move-update-to-prefix.sh \
	update/xdelta/50-symlinks.sh \
	update/xdelta/60-desktop-updates.sh \
	update/xdelta/60-python-compile.sh \
	update/xdelta/70-cleanup.sh \
	update/rollback/00-directories.sh \
	update/rollback/01-delete-symlinks.sh \
	update/rollback/02-move-back-installation-dir.sh \
	update/rollback/03-symlinks.sh \
	update/rollback/04-desktop-updates.sh \
	update/rollback/04-python-compile.sh \
	update/rollback/05-cleanup.sh \
	$(NULL)

pkglibexec_SCRIPTS = \
	$(common_scripts) \
	$(NULL)

nobase_pkglibexec_SCRIPTS = \
	$(update_scripts) \
	$(NULL)

EXTRA_DIST = \
	$(common_scripts) \
	$(update_scripts) \
	$(NULL)
