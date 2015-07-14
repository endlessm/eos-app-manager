/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_CONFIG_H
#define EAM_CONFIG_H

#include <glib.h>

G_BEGIN_DECLS

const char *    eam_config_get_applications_dir         (void);
const char *    eam_config_get_download_dir             (void);
const char *    eam_config_get_server_address           (void);
const char *    eam_config_get_protocol_version         (void);
const char *    eam_config_get_script_dir               (void);
const char *    eam_config_get_gpg_keyring              (void);
guint           eam_config_get_inactivity_timeout       (void);
gboolean        eam_config_get_delta_updates_enabled    (void);

#define eam_config_appdir       eam_config_get_applications_dir
#define eam_config_dldir        eam_config_get_download_dir
#define eam_config_saddr        eam_config_get_server_address
#define eam_config_protver      eam_config_get_protocol_version
#define eam_config_scriptdir    eam_config_get_script_dir
#define eam_config_gpgkeyring   eam_config_get_gpg_keyring
#define eam_config_timeout      eam_config_get_inactivity_timeout
#define eam_config_deltaupdates eam_config_get_delta_updates_enabled

G_END_DECLS

#endif /* EAM_CONFIG_H */
