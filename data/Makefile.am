RESOURCE_FILES = \
	eam-dbus-interface.xml \
	$(NULL)

dbusservicedir       = $(datadir)/dbus-1/system-services
dbusservice_in_files = com.endlessm.AppManager.service.in
dbusservice_DATA     = $(dbusservice_in_files:.service.in=.service)

$(dbusservice_DATA): $(dbusservice_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" -e "s|\@EAM_USER\@|$(EAM_USER)|" $< > $@

dbusconfdir = $(sysconfdir)/dbus-1/system.d
dbusconf_in_files = com.endlessm.AppManager.conf.in
dbusconf_DATA = $(dbusconf_in_files:.conf.in=.conf)

systemdservice_in_files = eos-app-manager.service.in
systemdservicedir = $(systemdsystemunitdir)
systemdservice_DATA = $(systemdservice_in_files:.service.in=.service)

$(systemdservice_DATA): $(systemdservice_in_files) $(systemdsystemunitdir) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@

xsessiondir = $(sysconfdir)/X11/Xsession.d
xsession_DATA = 65eam_xdg_data_dirs_update

$(dbusconf_DATA): $(dbusconf_in_files) Makefile
	@sed -e "s|\@EAM_USER\@|$(EAM_USER)|" $< > $@

conf_in_files = eam-default.cfg.in
confdir = $(sysconfdir)/eos-app-manager
conf_DATA = $(conf_in_files:.cfg.in=.cfg)

$(conf_DATA): $(conf_in_files) Makefile
	@sed -e "s|\@pkglibexecdir\@|$(pkglibexecdir)|" -e "s|\@localstatedir\@|$(localstatedir)|" $< > $@

polkit_rules_in_files = com.endlessm.app-installer.rules.in
polkit_rulesdir = $(datadir)/polkit-1/rules.d
dist_polkit_rules_DATA = $(polkit_rules_in_files:.rules.in=.rules)

$(dist_polkit_rules_DATA): $(polkit_rules_in_files)
	@sed -e "s|\@EAM_ADMIN_GROUP\@|$(EAM_ADMIN_GROUP)|" $< > $@

polkit_policydir = $(datadir)/polkit-1/actions
dist_polkit_policy_DATA = com.endlessm.app-installer.policy

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
	$(NULL)

DISTCLEANFILES = \
	$(dbusservice_DATA) \
	$(systemdservice_DATA) \
	$(dbusconf_DATA) \
	$(conf_DATA) \
	$(dist_polkit_rules_DATA) \
	$(NULL)

-include $(top_srcdir)/git.mk
