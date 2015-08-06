/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_CONFIG_H
#define EAM_CONFIG_H

#include <glib.h>

G_BEGIN_DECLS

const char *    eam_config_get_applications_dir         (void);
const char *    eam_config_get_cache_dir                (void);
const char *    eam_config_get_primary_storage          (void);
const char *    eam_config_get_secondary_storage        (void);
const char *    eam_config_get_gpg_keyring              (void);
const char *    eam_config_get_server_url               (void);
const char *    eam_config_get_api_version              (void);
gboolean        eam_config_get_enable_delta_updates     (void);
guint           eam_config_get_inactivity_timeout       (void);

gboolean        eam_config_set_key                      (const char *key,
                                                         const char *value);
gboolean        eam_config_reset_key                    (const char *key);

G_END_DECLS

#endif /* EAM_CONFIG_H */
