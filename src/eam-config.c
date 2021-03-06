/* eam-config.c: Configuration API
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

#include <string.h>
#include <glib-object.h>

#include "eam-config.h"
#include "eam-log.h"

#define EAM_CONFIG_DIRECTORIES  "Directories"
#define EAM_CONFIG_DAEMON       "Daemon"
#define EAM_CONFIG_REPOSITORY   "Repository"

typedef struct {
  /* Directories */
  char *apps_root;
  char *cache_dir;
  char *primary_storage;
  char *secondary_storage;
  char *gpgkeyring;

  /* Repository */
  char *server_url;
  char *api_version;
  gboolean enable_delta_updates;

  /* Daemon */
  guint inactivity_timeout;
} EamConfig;

typedef struct {
  const char *key_name;
  const char *key_group;
  gsize key_field;
  GType key_type;
  union {
    int int_val;
    const char *str_val;
    gboolean bool_val;
  } key_default;
} EamConfigKey;

static const EamConfigKey eam_config_keys[] = {
  {
    .key_name = "ApplicationsDir",
    .key_group = EAM_CONFIG_DIRECTORIES,
    .key_field = G_STRUCT_OFFSET (EamConfig, apps_root),
    .key_type = G_TYPE_STRING,
    .key_default.str_val = "/endless",
  },
  {
    .key_name = "CacheDir",
    .key_group = EAM_CONFIG_DIRECTORIES,
    .key_field = G_STRUCT_OFFSET (EamConfig, cache_dir),
    .key_type = G_TYPE_STRING,
    .key_default.str_val = LOCALSTATEDIR "/cache/eos-app-manager",
  },
  {
    .key_name = "PrimaryStorage",
    .key_group = EAM_CONFIG_DIRECTORIES,
    .key_field = G_STRUCT_OFFSET (EamConfig, primary_storage),
    .key_type = G_TYPE_STRING,
    .key_default.str_val = LOCALSTATEDIR "/endless",
  },
  {
    .key_name = "SecondaryStorage",
    .key_group = EAM_CONFIG_DIRECTORIES,
    .key_field = G_STRUCT_OFFSET (EamConfig, secondary_storage),
    .key_type = G_TYPE_STRING,
    .key_default.str_val = LOCALSTATEDIR "/endless-extra",
  },
  {
    .key_name = "GpgKeyring",
    .key_group = EAM_CONFIG_DIRECTORIES,
    .key_field = G_STRUCT_OFFSET (EamConfig, gpgkeyring),
    .key_type = G_TYPE_STRING,
    .key_default.str_val = PKGDATADIR "/eos-keyring.gpg",
  },
  {
    .key_name = "InactivityTimeout",
    .key_group = EAM_CONFIG_DAEMON,
    .key_field = G_STRUCT_OFFSET (EamConfig, inactivity_timeout),
    .key_type = G_TYPE_INT,
    .key_default.int_val = 300,
  },
  {
    .key_name = "ServerUrl",
    .key_group = EAM_CONFIG_REPOSITORY,
    .key_field = G_STRUCT_OFFSET (EamConfig, server_url),
    .key_type = G_TYPE_STRING,
    .key_default.str_val = "https://appupdates.endlessm.com",
  },
  {
    .key_name = "ApiVersion",
    .key_group = EAM_CONFIG_REPOSITORY,
    .key_field = G_STRUCT_OFFSET (EamConfig, api_version),
    .key_type = G_TYPE_STRING,
    .key_default.str_val = "v1",
  },
  {
    .key_name = "EnableDeltaUpdates",
    .key_group = EAM_CONFIG_REPOSITORY,
    .key_field = G_STRUCT_OFFSET (EamConfig, enable_delta_updates),
    .key_type = G_TYPE_BOOLEAN,
    .key_default.bool_val = TRUE,
  },
};

static inline void
eam_config_set_key_internal (const EamConfigKey *key,
                             EamConfig          *config,
                             GKeyFile           *keyfile)
{
  g_autoptr(GError) error = NULL;
  g_autofree char *str_value = NULL;
  int int_value = 0;
  gboolean bool_value = FALSE;
  gpointer field_p = G_STRUCT_MEMBER_P (config, key->key_field);

  switch (key->key_type) {
    case G_TYPE_STRING:
      str_value = g_key_file_get_string (keyfile, key->key_group, key->key_name, &error);
      if (error == NULL)
        (* (gpointer *) field_p) = g_strdup (str_value);
      else
        (* (gpointer *) field_p) = g_strdup (key->key_default.str_val);
      break;

    case G_TYPE_INT:
      int_value = g_key_file_get_integer (keyfile, key->key_group, key->key_name, &error);
      if (error == NULL)
        (* (guint *) field_p) = int_value;
      else
        (* (guint *) field_p) = key->key_default.int_val;
      break;

    case G_TYPE_BOOLEAN:
      bool_value = g_key_file_get_boolean (keyfile, key->key_group, key->key_name, &error);
      if (error == NULL)
        (* (gboolean *) field_p) = bool_value;
      else
        (* (gboolean *) field_p) = key->key_default.bool_val;
      break;

    default:
      g_assert_not_reached ();
  }

  if (error != NULL)
    eam_log_error_message ("Unable to read configuration key '%s': %s",
                           key->key_name,
                           error->message);
}

static EamConfig *
eam_config_get (void)
{
  static volatile gpointer eam_config;

  if (g_once_init_enter (&eam_config)) {
    const char *config_env = g_getenv ("EAM_CONFIG_FILE");
    if (config_env == NULL)
      config_env = SYSCONFDIR "/eos-app-manager/eos-app-manager.ini";

    EamConfig *config = g_new (EamConfig, 1);

    g_autoptr(GKeyFile) keyfile = g_key_file_new ();
    g_autoptr(GError) error = NULL;
    g_key_file_load_from_file (keyfile, config_env, 0, &error);

    if (error != NULL)
      eam_log_error_message ("Unable to load configuration from '%s': %s",
                             config_env,
                             error->message);

    for (int i = 0; i < G_N_ELEMENTS (eam_config_keys); i++) {
      const EamConfigKey *key = &eam_config_keys[i];

      eam_config_set_key_internal (key, config, keyfile);
    }

    g_once_init_leave (&eam_config, config);
  }

  return eam_config;
}

gboolean
eam_config_set_key (const char *key,
                    const char *value)
{
  EamConfig *config = eam_config_get ();

  const char *config_env = g_getenv ("EAM_CONFIG_FILE");
  if (config_env == NULL)
    config_env = SYSCONFDIR "/eos-app-manager/eos-app-manager.ini";

  g_autoptr(GKeyFile) keyfile = g_key_file_new ();
  g_autoptr(GError) error = NULL;
  g_key_file_load_from_file (keyfile, config_env, G_KEY_FILE_KEEP_COMMENTS, &error);

  if (error != NULL) {
    eam_log_error_message ("Unable to load configuration from '%s': %s",
                           config_env,
                           error->message);
    return FALSE;
  }

  for (int i = 0; i < G_N_ELEMENTS (eam_config_keys); i++) {
    const EamConfigKey *config_key = &eam_config_keys[i];

    if (strcmp (key, config_key->key_name) == 0) {
      g_key_file_set_value (keyfile, config_key->key_group, config_key->key_name, value);
      g_key_file_save_to_file (keyfile, config_env, &error);
      if (error != NULL) {
          eam_log_error_message ("Unable to save configuration to '%s': %s",
                                 config_env,
                                 error->message);
          return FALSE;
      }

      eam_config_set_key_internal (config_key, config, keyfile);
      return TRUE;
    }
  }

  /* Key wasn't found */
  eam_log_error_message ("Unknown configuration key '%s'", key);
  return FALSE;
}

gboolean
eam_config_reset_key (const char *key)
{
  EamConfig *config = eam_config_get ();

  const char *config_env = g_getenv ("EAM_CONFIG_FILE");
  if (config_env == NULL)
    config_env = SYSCONFDIR "/eos-app-manager/eos-app-manager.ini";

  g_autoptr(GKeyFile) keyfile = g_key_file_new ();
  g_autoptr(GError) error = NULL;
  g_key_file_load_from_file (keyfile, config_env, G_KEY_FILE_KEEP_COMMENTS, &error);

  if (error != NULL) {
    eam_log_error_message ("Unable to load configuration from '%s': %s",
                           config_env,
                           error->message);
    return FALSE;
  }

  for (int i = 0; i < G_N_ELEMENTS (eam_config_keys); i++) {
    const EamConfigKey *config_key = &eam_config_keys[i];

    if (strcmp (key, config_key->key_name) == 0) {
      switch (config_key->key_type) {
        case G_TYPE_STRING:
          g_key_file_set_string (keyfile,
                                 config_key->key_group,
                                 config_key->key_name,
                                 config_key->key_default.str_val);
          break;

        case G_TYPE_INT:
          g_key_file_set_integer (keyfile,
                                  config_key->key_group,
                                  config_key->key_name,
                                  config_key->key_default.int_val);
          break;

        case G_TYPE_BOOLEAN:
          g_key_file_set_boolean (keyfile,
                                  config_key->key_group,
                                  config_key->key_name,
                                  config_key->key_default.bool_val);
          break;

        default:
          g_assert_not_reached ();
      }

      g_key_file_save_to_file (keyfile, config_env, &error);
      if (error != NULL) {
        eam_log_error_message ("Unable to save configuration to '%s': %s",
                               config_env,
                               error->message);
        return FALSE;
      }

      eam_config_set_key_internal (config_key, config, keyfile);
      return TRUE;
    }
  }

  /* Key wasn't found */
  eam_log_error_message ("Unknown configuration key '%s'", key);
  return FALSE;
}

const char *
eam_config_get_applications_dir (void)
{
  return eam_config_get ()->apps_root;
}

const char *
eam_config_get_cache_dir (void)
{
  return eam_config_get ()->cache_dir;
}

const char *
eam_config_get_gpg_keyring (void)
{
  return eam_config_get ()->gpgkeyring;
}

const char *
eam_config_get_primary_storage (void)
{
  return eam_config_get ()->primary_storage;
}

const char *
eam_config_get_secondary_storage (void)
{
  return eam_config_get ()->secondary_storage;
}

guint
eam_config_get_inactivity_timeout (void)
{
  return eam_config_get ()->inactivity_timeout;
}

const char *
eam_config_get_server_url (void)
{
  return eam_config_get ()->server_url;
}

const char *
eam_config_get_api_version (void)
{
  return eam_config_get ()->api_version;
}

gboolean
eam_config_get_enable_delta_updates (void)
{
  return eam_config_get ()->enable_delta_updates;
}
