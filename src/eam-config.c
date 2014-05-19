/* Copyright 2014 Endless Mobile, Inc. */

#include <gio/gio.h>
#include "eam-config.h"

struct _EamConfig
{
#define GETTERS(p) gchar *p;
  PARAMS_LIST(GETTERS)
#undef GETTERS
  guint timeout;
};

static gpointer
eam_config_init (gpointer data)
{
  EamConfig *cfg = g_slice_new0 (EamConfig);
  eam_config_load (cfg, NULL);
  return cfg;
}

EamConfig *
eam_config_get (void)
{
  static GOnce once_init = G_ONCE_INIT;
  return g_once (&once_init, eam_config_init, NULL);
}

void
eam_config_set (EamConfig *cfg, gchar *appdir, gchar *dldir,
  gchar *saddr, gchar *protver, gchar *scriptdir, guint timeout)
{
  if (!cfg)
    cfg = eam_config_get ();

  if (!cfg)
    return;

#define SETTERS(p) if (p) { g_free (cfg->p); cfg->p=p; }
  PARAMS_LIST(SETTERS)
#undef SETTERS

  if (timeout > 0)
    cfg->timeout = timeout;
}

void
eam_config_free (EamConfig *cfg)
{
  if (!cfg)
    cfg = eam_config_get ();

  if (!cfg)
    return;

#define CLEARERS(p) g_clear_pointer (&cfg->p, g_free);
  PARAMS_LIST(CLEARERS)
#undef CLEARERS
}

void
eam_config_destroy (EamConfig *cfg)
{
  if (!cfg)
    cfg = eam_config_get ();

  if (!cfg)
    return;

  eam_config_free (cfg);
  g_slice_free (EamConfig, cfg);
}

static inline gchar *
get_str (GKeyFile *keyfile, gchar *grp, gchar *key)
{
  GError *err = NULL;
  gchar *value = g_key_file_get_string (keyfile, grp, key, &err);
  if (err) {
    g_free (value);
    g_error_free (err);
    return NULL;
  }

  return g_strstrip (value);
}

static GKeyFile *
load_default (void)
{
  GKeyFile *keyfile = g_key_file_new ();
  gboolean loaded = g_key_file_load_from_file (keyfile,
     SYSCONFDIR "/eos-app-manager/eam-default.cfg",
     G_KEY_FILE_NONE, NULL);

  if (!loaded) {
    g_key_file_unref (keyfile);
    return NULL;
  }

  g_debug ("Loaded configuration from %s\n", SYSCONFDIR "/eos-app-manager/eam-default.cfg");

  return keyfile;
}

gboolean
eam_config_load (EamConfig *cfg, GKeyFile *keyfile)
{
  g_return_val_if_fail (cfg, FALSE);

  gboolean ret = FALSE;
  gboolean own = FALSE;
  gchar *grp, *appdir, *saddr, *dldir, *protver, *scriptdir;
  guint timeout;

  if (!keyfile) {
    if (!(keyfile = load_default ()))
      return FALSE;
    own = TRUE;
  }

  grp = g_key_file_get_start_group (keyfile);
  appdir = get_str (keyfile, grp, "appdir");
  saddr = get_str (keyfile, grp, "serveraddress");
  dldir = get_str (keyfile, grp, "downloaddir");
  protver = get_str (keyfile, grp, "protocolversion");
  scriptdir = get_str (keyfile, grp, "scriptdir");
  timeout = g_key_file_get_integer (keyfile, grp, "timeout", NULL);

  eam_config_set (cfg, appdir, dldir, saddr, protver, scriptdir, timeout);

  ret = TRUE;

  g_free (grp);

  if (own)
    g_key_file_unref (keyfile);

  return ret;
}

void
eam_config_dump (EamConfig *cfg)
{
  if (!cfg)
    cfg = eam_config_get ();

  g_print ("EAM Configuration:\n\n"
    "\tAppDir = %s\n"
    "\tServerAddress = %s\n"
    "\tDownloadDir = %s\n"
    "\tProtocolVersion = %s\n"
    "\tScriptDir = %s\n"
    "\tTimeOut = %d\n",
    cfg->appdir, cfg->saddr, cfg->dldir, cfg->protver, cfg->scriptdir,
    cfg->timeout);
}

guint
eam_config_timeout ()
{
  return eam_config_get ()->timeout;
}

#define GETTERS(p) \
  const gchar *eam_config_##p () { return eam_config_get ()->p; }
PARAMS_LIST(GETTERS)
#undef GETTERS
