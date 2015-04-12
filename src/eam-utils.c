/* Copyright 2014 Endless Mobile, Inc. */
#include "config.h"

#include <string.h>
#include <glib/gi18n.h>

#include "eam-utils.h"

#include "eam-config.h"
#include "eam-error.h"
#include "eam-spawner.h"

#define BUNDLE_SIGNATURE_EXT ".asc"
#define BUNDLE_HASH_EXT ".sha256"

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

void
eam_utils_run_bundle_scripts (const gchar *appid, const gchar *filename,
  const gchar *scriptdir,
  GCancellable *cancellable, GAsyncReadyCallback callback, GTask *task)
{
  /* scripts directory path */
  char *dir = g_build_filename (eam_config_scriptdir (), scriptdir, NULL);

  /* scripts parameters */
  GStrv params = g_new (gchar *, 3);
  params[0] = g_strdup (appid);
  params[1] = g_strdup (filename);
  params[2] = NULL;

  /* prefix environment */
  GHashTable *env = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (env, (gpointer) "EAM_PREFIX", (gpointer) eam_config_appdir ());
  g_hash_table_insert (env, (gpointer) "EAM_TMP", (gpointer) eam_config_dldir ());
  g_hash_table_insert (env, (gpointer) "EAM_GPGKEYRING", (gpointer) eam_config_gpgkeyring ());

  EamSpawner *spawner = eam_spawner_new (dir, env, (const gchar * const *) params);
  eam_spawner_run_async (spawner, cancellable, callback, task);

  g_free (dir);
  g_strfreev (params);
  g_object_unref (spawner);
}
