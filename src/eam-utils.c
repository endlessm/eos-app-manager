/* Copyright 2014 Endless Mobile, Inc. */
#include "config.h"

#include <string.h>
#include <glib/gi18n.h>

#include "eam-utils.h"

#include "eam-config.h"
#include "eam-error.h"
#include "eam-os.h"
#include "eam-spawner.h"

#define BUNDLE_SIGNATURE_EXT ".asc"
#define BUNDLE_HASH_EXT ".sha256"

gboolean
eam_utils_appid_is_legal (const char *appid)
{
  static const char alsoallowed[] = "-+.";
  static const char *reserveddirs[] = { "bin", "share", "lost+found" };

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

char *
eam_utils_build_tarball_filename (const char *bundle_location, const char *appid,
  const char *extension)
{
  if (bundle_location != NULL)
    return g_strdup (bundle_location);

  gchar *fname = NULL;
  fname = g_strconcat (appid, extension, NULL);

  gchar *ret = g_build_filename (eam_config_dldir (), fname, NULL);
  g_free (fname);

  return ret;
}

static char *
eam_utils_build_bundle_filename (const char *bundle_location, const char *appid,
  const char *extension)
{
  gchar *dirname;

  if (bundle_location != NULL)
    dirname = g_path_get_dirname (bundle_location);
  else
    dirname = g_strdup (eam_config_dldir ());

  gchar *fname = g_strconcat (appid, extension, NULL);
  gchar *ret = g_build_filename (dirname, fname, NULL);

  g_free (fname);
  g_free (dirname);

  return ret;
}

void
eam_utils_create_bundle_hash_file (const char *hash, const char *tarball,
  const char *bundle_location, const char *appid, GError **error)
{
  gchar *contents = g_strconcat (hash, "\t", tarball, "\n", NULL);
  gchar *filename = eam_utils_build_bundle_filename (bundle_location, appid,
                                                     BUNDLE_HASH_EXT);

  g_file_set_contents (filename, contents, -1, error);

  g_free (filename);
  g_free (contents);
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
