/* Copyright 2014 Endless Mobile, Inc. */

#include <string.h>
#include <glib-object.h>

#include "eam-config.h"
#include "eam-log.h"

#define EAM_CONFIG_GROUP        "eam"

typedef struct {
  char *appdir;
  char *dldir;
  char *saddr;
  char *protver;
  char *scriptdir;
  char *gpgkeyring;

  guint timeout;

  gboolean deltaupdates;
} EamConfig;

/* The defaults we fall back to */
static const EamConfig eam_config_default = {
  .appdir = "/endless",
  .dldir = LOCALSTATEDIR "/endless",
  .saddr = "http://appupdates.endlessm.com",
  .protver = "v1",
  .scriptdir = PKGLIBEXECDIR,
  .gpgkeyring = PKGDATADIR "/eos-keyring.gpg",
  .timeout = 300,
  .deltaupdates = FALSE,
};

typedef struct {
  const char *key_name;
  gsize key_field;
  GType key_type;
} EamConfigKey;

static const EamConfigKey eam_config_keys[] = {
  {
    .key_name = "appdir",
    .key_field = G_STRUCT_OFFSET (EamConfig, appdir),
    .key_type = G_TYPE_STRING,
  },
  {
    .key_name = "downloadir",
    .key_field = G_STRUCT_OFFSET (EamConfig, dldir),
    .key_type = G_TYPE_STRING,
  },
  {
    .key_name = "serveraddress",
    .key_field = G_STRUCT_OFFSET (EamConfig, saddr),
    .key_type = G_TYPE_STRING,
  },
  {
    .key_name = "protocolversion",
    .key_field = G_STRUCT_OFFSET (EamConfig, protver),
    .key_type = G_TYPE_STRING,
  },
  {
    .key_name = "scriptdir",
    .key_field = G_STRUCT_OFFSET (EamConfig, scriptdir),
    .key_type = G_TYPE_STRING,
  },
  {
    .key_name = "gpgkeyring",
    .key_field = G_STRUCT_OFFSET (EamConfig, gpgkeyring),
    .key_type = G_TYPE_STRING,
  },
  {
    .key_name = "timeout",
    .key_field = G_STRUCT_OFFSET (EamConfig, timeout),
    .key_type = G_TYPE_INT,
  },
  {
    .key_name = "deltaupdates",
    .key_field = G_STRUCT_OFFSET (EamConfig, deltaupdates),
    .key_type = G_TYPE_BOOLEAN,
  }
};

static inline void
eam_config_set_key (const EamConfigKey *key,
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
      str_value = g_key_file_get_string (keyfile, EAM_CONFIG_GROUP, key->key_name, &error);
      if (error == NULL)
        (* (gpointer *) field_p) = str_value;
      else
        (* (gpointer *) field_p) = G_STRUCT_MEMBER (char *, &eam_config_default, key->key_field);
      break;

    case G_TYPE_BOOLEAN:
      int_value = g_key_file_get_integer (keyfile, EAM_CONFIG_GROUP, key->key_name, &error);
      if (error == NULL)
        (* (guint *) field_p) = int_value;
      else
        (* (guint *) field_p) = G_STRUCT_MEMBER (guint, &eam_config_default, key->key_field);
      break;

    case G_TYPE_INT:
      bool_value = g_key_file_get_boolean (keyfile, EAM_CONFIG_GROUP, key->key_name, &error);
      if (error == NULL)
        (* (gboolean *) field_p) = bool_value;
      else
        (* (gboolean *) field_p) = G_STRUCT_MEMBER (gboolean, &eam_config_default, key->key_field);
      break;

    default:
      g_assert_not_reached ();
  }

  if (error != NULL) {
    eam_log_error_message ("Unable to read configuration key '%s': %s",
                           key->key_name,
                           error->message);
  }
}

static EamConfig *
eam_config_get (void)
{
  static volatile gpointer eam_config;

  if (g_once_init_enter (&eam_config)) {
    const char *config_env = g_getenv ("EAM_CONFIG_FILE");
    if (config_env == NULL)
      config_env = SYSCONFDIR "/eos-app-manager/eam-default.cfg";

    EamConfig *config = g_new (EamConfig, 1);
    memcpy (config, &eam_config_default, sizeof (EamConfig));

    g_autoptr(GKeyFile) keyfile = g_key_file_new ();
    g_autoptr(GError) error = NULL;
    g_key_file_load_from_file (keyfile, config_env, 0, &error);
    if (error != NULL) {
      eam_log_error_message ("Unable to load configuration from '%s': %s",
                             config_env,
                             error->message);
    }
    else {
      for (int i = 0; i < G_N_ELEMENTS (eam_config_keys); i++) {
        const EamConfigKey *key = &eam_config_keys[i];

        eam_config_set_key (key, config, keyfile);
      }
    }

    g_once_init_leave (&eam_config, config);
  }

  return eam_config;
}

const char *
eam_config_get_applications_dir (void)
{
  return eam_config_get ()->appdir;
}

const char *
eam_config_get_server_address (void)
{
  return eam_config_get ()->saddr;
}

const char *
eam_config_get_download_dir (void)
{
  return eam_config_get ()->dldir;
}

const char *
eam_config_get_protocol_version (void)
{
  return eam_config_get ()->protver;
}

const char *
eam_config_get_script_dir (void)
{
  return eam_config_get ()->scriptdir;
}

const char *
eam_config_get_gpg_keyring (void)
{
  return eam_config_get ()->gpgkeyring;
}

guint
eam_config_get_inactivity_timeout (void)
{
  return eam_config_get ()->timeout;
}

gboolean
eam_config_get_delta_updates_enabled (void)
{
  return eam_config_get ()->deltaupdates;
}
