RESOURCE_FILES = \
	eam-dbus-interface.xml \
	eam-default.cfg \
	$(NULL)

dbusservicedir       = $(datadir)/dbus-1/system-services
dbusservice_in_files = com.Endless.AppManager.service.in
dbusservice_DATA     = $(dbusservice_in_files:.service.in=.service)

$(dbusservice_DATA): $(dbusservice_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" -e "s|\@EAM_USER\@|$(EAM_USER)|" $< > $@

dbusconfdir = $(sysconfdir)/dbus-1/system.d
dbusconf_in_files = com.Endless.AppManager.conf.in
dbusconf_DATA = $(dbusconf_in_files:.conf.in=.conf)

xsessiondir = $(sysconfdir)/X11/Xsession.d
xsession_DATA = 65eam_xdg_data_dirs_update

$(dbusconf_DATA): $(dbusconf_in_files) Makefile
	@sed -e "s|\@EAM_USER\@|$(EAM_USER)|" $< > $@

EXTRA_DIST = \
	$(RESOURCE_FILES) \
	eam.gresource.xml \
	$(dbusservice_in_files) \
	$(dbusconf_in_files) \
	65eam_xdg_data_dirs_update \
	$(NULL)

DISTCLEANFILES = \
	$(dbusservice_DATA) \
	$(dbusconf_DATA) \
	$(NULL)
