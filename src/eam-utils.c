/* Copyright 2014 Endless Mobile, Inc. */
#include "config.h"

#include <string.h>
#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "eam-utils.h"

#include "eam-config.h"
#include "eam-error.h"
#include "eam-os.h"
#include "eam-rest.h"
#include "eam-spawner.h"
#include "eam-version.h"
#include "eam-wc.h"

#define BUNDLE_SIGNATURE_EXT ".asc"
#define BUNDLE_HASH_EXT ".sha256"

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
eam_utils_download_bundle_signature (GTask *task, GAsyncReadyCallback callback,
  const char *signature_url, const char *bundle_location,
  const char *appid)
{
  /* download signature */
  /* @TODO: make all downloads in parallel */
  gchar *filename =  eam_utils_build_bundle_filename (bundle_location, appid,
                                                      BUNDLE_SIGNATURE_EXT);

  GCancellable *cancellable = g_task_get_cancellable (task);
  EamWc *wc = eam_wc_new ();
  eam_wc_request_async (wc, signature_url, filename, cancellable, callback,
                        task);

  g_object_unref (wc);
  g_free (filename);
}

void
eam_utils_download_bundle (GTask *task, GAsyncReadyCallback callback,
  const char *download_url, const char *bundle_location, const char *appid,
  const char *extension)
{
  gchar *filename = NULL;

  filename = eam_utils_build_tarball_filename (bundle_location, appid, extension);

  EamWc *wc = eam_wc_new ();
  GCancellable *cancellable = g_task_get_cancellable (task);
  eam_wc_request_async (wc, download_url, filename, cancellable,
                        callback, task);

  g_object_unref (wc);
  g_free (filename);
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

/* Returns: 0 if both versions are equal, <0 if a is smaller than b,
 * >0 if a is greater than b. */
int
eam_utils_compare_bundle_json_version (JsonObject *a, JsonObject *b)
{
  if (!a)
    return -(a != b);

  if (!b)
    return a != b;

  const gchar *a_version = json_object_get_string_member (a, "codeVersion");
  const gchar *b_version = json_object_get_string_member (b, "codeVersion");

  if (!a_version)
    return -(a_version != b_version);

  if (!b_version)
    return a_version != b_version;

  EamPkgVersion *a_pkg_version = eam_pkg_version_new_from_string (a_version);
  EamPkgVersion *b_pkg_version = eam_pkg_version_new_from_string (b_version);

  gint result = eam_pkg_version_compare (a_pkg_version, b_pkg_version);

  eam_pkg_version_free (a_pkg_version);
  eam_pkg_version_free (b_pkg_version);

  return result;
}

void
eam_utils_request_json_updates (GTask *task, GAsyncReadyCallback callback,
  const char *appid)
{
  /* is it enough? */
  if (!g_network_monitor_get_network_available (g_network_monitor_get_default ())) {
    g_task_return_new_error (task, EAM_ERROR,
      EAM_ERROR_NO_NETWORK, _("Networking is not available"));
    goto bail;
  }

  gchar *uri = eam_rest_build_uri (EAM_REST_API_V1_GET_APP_UPDATE_LINK,
                                   appid, NULL, NULL);
  if (!uri) {
    g_task_return_new_error (task, EAM_ERROR,
      EAM_ERROR_PROTOCOL_ERROR, _("Not valid method or protocol version"));
    goto bail;
  }

  EamWc *wc = eam_wc_new ();
  GCancellable *cancellable = g_task_get_cancellable (task);
  eam_wc_request_instream_with_headers_async (wc, uri, cancellable, callback,
    task, "Accept", "application/json", "personality",
    eam_os_get_personality (), NULL);

  g_free (uri);
  g_object_unref (wc);

  return;
bail:
  g_object_unref (task);
}

gchar *
eam_utils_get_json_updates_filename (void)
{
  return g_build_filename (eam_config_dldir (), "updates.json", NULL);
}
