/* eam-config.h: Configuration API
 *
 * This file is part of eos-app-manager.
 * Copyright 2014  Endless Mobile Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
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
