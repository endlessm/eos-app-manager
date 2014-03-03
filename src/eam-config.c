/* Copyright 2014 Endless Mobile, Inc. */

#include <gio/gio.h>
#include "eam-config.h"

static gpointer
eam_config_allocate (gpointer data)
{
  return g_new0 (EamConfig, 1);
}

EamConfig *
eam_config_get (void)
{
  static GOnce once_init = G_ONCE_INIT;
  return g_once (&once_init, eam_config_allocate, NULL);
}

void
eam_config_set (EamConfig *cfg, gchar *appdir, gchar *serveraddr,
  gchar *dldir)
{
  if (appdir) {
    g_free (cfg->appdir);
    cfg->appdir = appdir;
  }

  if (serveraddr) {
    g_free (cfg->serveraddr);
    cfg->serveraddr = serveraddr;
  }

  if (dldir) {
    g_free (cfg->dldir);
    cfg->dldir = dldir;
  }
}

void
eam_config_free (EamConfig *cfg)
{
  if (!cfg)
    cfg = eam_config_get ();

  if (!cfg)
    return;

  g_free (cfg->appdir);
  g_free (cfg->serveraddr);
  g_free (cfg->dldir);
  g_free (cfg);
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

  return value;
}

static GKeyFile *
load_default (void)
{
  GBytes *data = g_resources_lookup_data ("/com/Endless/AppManager/eam-default.cfg",
    G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (!data)
    return NULL;

  GKeyFile *keyfile = g_key_file_new ();
  gboolean loaded = g_key_file_load_from_data (keyfile,
     g_bytes_get_data (data, NULL), -1, G_KEY_FILE_NONE, NULL);
  g_bytes_unref (data);
  if (!loaded) {
    g_key_file_unref (keyfile);
    return NULL;
  }

  return keyfile;
}

gboolean
eam_config_load (EamConfig *cfg, GKeyFile *keyfile)
{
  g_return_val_if_fail (cfg, FALSE);

  gboolean ret = FALSE;
  gboolean own = FALSE;
  gchar *grp, *appdir, *saddr, *dldir;

  if (!keyfile) {
    if (!(keyfile = load_default ()))
      return FALSE;
    own = TRUE;
  }

  grp = g_key_file_get_start_group (keyfile);
  appdir = get_str (keyfile, grp, "appdir");
  saddr = get_str (keyfile, grp, "serveraddress");
  dldir = get_str (keyfile, grp, "downloaddir");

  eam_config_set (cfg, appdir, saddr, dldir);

  ret = TRUE;

  g_free (grp);

  if (own)
    g_key_file_unref (keyfile);

  return ret;
}
