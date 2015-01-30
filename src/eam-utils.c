/* Copyright 2014 Endless Mobile, Inc. */
#include "config.h"

#include <string.h>

#include "eam-utils.h"
#include "eam-config.h"
#include "eam-spawner.h"
#include "eam-wc.h"

gboolean
eam_utils_appid_is_legal (const char *appid)
{
  static const char alsoallowed[] = "-+.";
  static const char *reserveddirs[] = { "bin", "share" };

  if (!appid || appid[0] == '\0')
    return FALSE;

  guint i;
  for (i = 0; i < G_N_ELEMENTS(reserveddirs); i++) {
    if (g_strcmp0(appid, reserveddirs[i]) == 0)
      return FALSE;
  }

  if (!g_ascii_isalnum (appid[0]))
    return FALSE; /* must start with an alphanumeric character */

  int c;
  while ((c = *appid++) != '\0')
    if (!g_ascii_isalnum (c) && !strchr (alsoallowed, c))
      break;

  if (!c)
    return TRUE;

  return FALSE;
}

static char *
eam_utils_build_sign_filename (const char *bundle_location, const char *appid)
{
  gchar *dirname;

  if (bundle_location != NULL)
    dirname = g_path_get_dirname (bundle_location);
  else
    dirname = g_strdup (eam_config_dldir ());

  gchar *fname = g_strconcat (appid, ".asc", NULL);
  gchar *ret = g_build_filename (dirname, fname, NULL);
  g_free (fname);
  g_free (dirname);

  return ret;
}

void
eam_utils_download_bundle_signature (GTask *task, GAsyncReadyCallback callback,
  const char *signature_url, const char *bundle_location,
  const char *appid)
{
  /* download signature */
  /* @TODO: make all downloads in parallel */
  gchar *filename =  eam_utils_build_sign_filename (bundle_location,
                                                    appid);

  GCancellable *cancellable = g_task_get_cancellable (task);
  EamWc *wc = eam_wc_new ();
  eam_wc_request_async (wc, signature_url, filename, cancellable, callback,
                        task);

  g_object_unref (wc);
  g_free (filename);
}

void
eam_utils_run_bundle_scripts (const gchar *appid, const gchar *filename,
  const gchar *scriptdir, const gboolean external_download,
  GCancellable *cancellable, GAsyncReadyCallback callback, GTask *task)
{
  /* scripts directory path */
  char *dir = g_build_filename (eam_config_scriptdir (), scriptdir, NULL);

  /* scripts parameters */
  GStrv params = g_new0 (gchar *, 3);
  params[0] = g_strdup (appid);
  params[1] = g_strdup (filename);

  /* prefix environment */
  g_setenv ("EAM_PREFIX", eam_config_appdir (), FALSE);
  g_setenv ("EAM_TMP", eam_config_dldir (), FALSE);
  g_setenv ("EAM_GPGKEYRING", eam_config_gpgkeyring (), FALSE);
  /* if (priv->bundle_location != NULL) */
  if (external_download)
    g_setenv ("EAM_EXTDOWNLOAD", "1", FALSE);

  EamSpawner *spawner = eam_spawner_new (dir, (const gchar * const *) params);
  eam_spawner_run_async (spawner, cancellable, callback, task);

  g_free (dir);
  g_strfreev (params);
  g_object_unref (spawner);
}
