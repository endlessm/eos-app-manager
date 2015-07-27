RESOURCE_FILES = \
	eam-service-interface.xml \
	eam-transaction-interface.xml \
	$(NULL)

do_subst = @sed \
	-e 's|@EAM_ADMIN_GROUP[@]|$(EAM_ADMIN_GROUP)|' \
	-e 's|@EAM_USER[@]|$(EAM_USER)|' \
	-e 's|@bindir[@]|$(bindir)|' \
	-e 's|@localstatedir[@]|$(localstatedir)|' \
	-e 's|@pkgdatadir[@]|$(pkgdatadir)|' \
	-e 's|@pkglibexecdir[@]|$(pkglibexecdir)|'

dbusservicedir       = $(datadir)/dbus-1/system-services
dbusservice_in_files = com.endlessm.AppManager.service.in
dbusservice_DATA     = $(dbusservice_in_files:.service.in=.service)

com.endlessm.AppManager.service: com.endlessm.AppManager.service.in Makefile
	$(do_subst) $< > $@

dbusconfdir = $(sysconfdir)/dbus-1/system.d
dbusconf_in_files = com.endlessm.AppManager.conf.in
dbusconf_DATA = $(dbusconf_in_files:.conf.in=.conf)

com.endlessm.AppManager.conf: com.endlessm.AppManager.conf.in Makefile
	$(do_subst) $< > $@

systemdservice_in_files = eos-app-manager.service.in eos-app-manager-migrate-bundles.service.in
systemdservicedir = $(systemdsystemunitdir)
systemdservice_DATA = $(systemdservice_in_files:.service.in=.service)

eos-app-manager.service: eos-app-manager.service.in Makefile
	$(do_subst) $< > $@

eos-app-manager-migrate-bundles.service: eos-app-manager-migrate-bundles.service.in Makefile
	$(do_subst) $< > $@

xsessiondir = $(sysconfdir)/X11/Xsession.d
xsession_DATA = 65eam_paths_update

conf_in_files = eam-default.cfg.in
confdir = $(sysconfdir)/eos-app-manager
conf_DATA = $(conf_in_files:.cfg.in=.cfg)

eam-default.cfg: eam-default.cfg.in Makefile
	$(do_subst) $< > $@

polkit_rules_in_files = com.endlessm.app-installer.rules.in
polkit_rulesdir = $(datadir)/polkit-1/rules.d
dist_polkit_rules_DATA = $(polkit_rules_in_files:.rules.in=.rules)

com.endlessm.app-installer.rules: com.endlessm.app-installer.rules.in Makefile
	$(do_subst) $< > $@

polkit_policydir = $(datadir)/polkit-1/actions
dist_polkit_policy_DATA = com.endlessm.app-installer.policy

tmpfilesdir = $(prefix)/lib/tmpfiles.d
tmpfiles_DATA = eos-app-manager.conf

man_MANS = eamd.8

EXTRA_DIST = \
	$(RESOURCE_FILES) \
	eam.gresource.xml \
	$(dbusservice_in_files) \
	$(systemdservice_in_files) \
	$(dbusconf_in_files) \
	$(conf_in_files) \
	$(xsession_DATA) \
	$(polkit_rules_in_files) \
	$(dist_polkit_policy_DATA) \
	eos-app-manager.conf \
	eamd.8 \
	$(NULL)

DISTCLEANFILES = \
	$(dbusservice_DATA) \
	$(systemdservice_DATA) \
	$(dbusconf_DATA) \
	$(conf_DATA) \
	$(dist_polkit_rules_DATA) \
	$(NULL)
