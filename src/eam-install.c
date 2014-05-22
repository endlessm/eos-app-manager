/* Copyright 2014 Endless Mobile, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "eam-install.h"
#include "eam-rest.h"
#include "eam-spawner.h"
#include "eam-wc.h"
#include "eam-config.h"
#include "eam-os.h"
#include "eam-version.h"

#define DEFAULT_SCRIPTDIR "install"
#define DEFAULT_ROLLBACK_SCRIPTDIR "install_rollback"

typedef struct _EamInstallPrivate	EamInstallPrivate;

struct _EamInstallPrivate
{
  gchar *appid;
  gchar *version;
  gchar *hash;
  gchar *scriptdir;
  gchar *rollback_scriptdir;
};

static void transaction_iface_init (EamTransactionInterface *iface);
static void eam_install_transaction_run_async (EamTransaction *trans, GCancellable *cancellable,
  GAsyncReadyCallback callback, gpointer data);
static gboolean eam_install_transaction_finish (EamTransaction *trans, GAsyncResult *res,
  GError **error);

G_DEFINE_TYPE_WITH_CODE (EamInstall, eam_install, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (EAM_TYPE_TRANSACTION, transaction_iface_init)
  G_ADD_PRIVATE (EamInstall));

enum
{
  PROP_APPID = 1,
  PROP_VERSION,
  PROP_SCRIPTDIR,
  PROP_ROLLBACK_SCRIPTDIR,
};

static void
transaction_iface_init (EamTransactionInterface *iface)
{
  iface->run_async = eam_install_transaction_run_async;
  iface->finish = eam_install_transaction_finish;
}

static void
eam_install_reset (EamInstall *self)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  g_clear_pointer (&priv->appid, g_free);
  g_clear_pointer (&priv->version, g_free);
  g_clear_pointer (&priv->hash, g_free);
  g_clear_pointer (&priv->scriptdir, g_free);
  g_clear_pointer (&priv->rollback_scriptdir, g_free);
}

static void
eam_install_finalize (GObject *obj)
{
  eam_install_reset (EAM_INSTALL (obj));
  G_OBJECT_CLASS (eam_install_parent_class)->finalize (obj);
}

static void
eam_install_set_property (GObject *obj, guint prop_id, const GValue *value,
  GParamSpec *pspec)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (EAM_INSTALL (obj));

  switch (prop_id) {
  case PROP_APPID:
    priv->appid = g_value_dup_string (value);
    break;
  case PROP_VERSION:
    priv->version = g_value_dup_string (value);
    break;
  case PROP_SCRIPTDIR:
    priv->scriptdir = g_value_dup_string (value);
    break;
  case PROP_ROLLBACK_SCRIPTDIR:
    priv->rollback_scriptdir = g_value_dup_string (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
eam_install_get_property (GObject *obj, guint prop_id, GValue *value,
  GParamSpec *pspec)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (EAM_INSTALL (obj));

  switch (prop_id) {
  case PROP_APPID:
    g_value_set_string (value, priv->appid);
    break;
  case PROP_VERSION:
    g_value_set_string (value, priv->version);
    break;
  case PROP_SCRIPTDIR:
    g_value_set_string (value, priv->scriptdir);
    break;
  case PROP_ROLLBACK_SCRIPTDIR:
    g_value_set_string (value, priv->rollback_scriptdir);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
eam_install_class_init (EamInstallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eam_install_finalize;
  object_class->get_property = eam_install_get_property;
  object_class->set_property = eam_install_set_property;

  /**
   * EamInstall:appid:
   *
   * The application ID to install.
   */
  g_object_class_install_property (object_class, PROP_APPID,
    g_param_spec_string ("appid", "App ID", "Application ID", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * EamInstall:version:
   *
   * The application version to install.
   */
  g_object_class_install_property (object_class, PROP_VERSION,
    g_param_spec_string ("version", "App version", "Application version", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * EamInstall:scriptdir:
   *
   * The path to the directory where the scripts to run during the installation process are
   */
  g_object_class_install_property (object_class, PROP_SCRIPTDIR,
    g_param_spec_string ("scriptdir", "Path to the installation scripts directory",
      "Path to the directory that contains the scripts to be run during the installation",
      DEFAULT_SCRIPTDIR, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * EamInstall:rollback-scriptdir:
   *
   * The path to the directory where the scripts to run if the installation process goes wrong
   * are.
   */
  g_object_class_install_property (object_class, PROP_ROLLBACK_SCRIPTDIR,
    g_param_spec_string ("rollback-scriptdir", "Path to the rollback installation scripts directory",
      "Path to the directory that contains the scripts to be run if the installation fails",
      DEFAULT_ROLLBACK_SCRIPTDIR, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
eam_install_init (EamInstall *self)
{
}

/**
 * eam_install_new:
 * @appid: the application ID to install.
 * @version: the target application version to install.
 *
 * Returns: a new instance of #EamInstall with #EamTransaction interface.
 */
EamTransaction *
eam_install_new (const gchar *appid, const gchar *version)
{
  return g_object_new (EAM_TYPE_INSTALL, "appid", appid, "version", version,
    NULL);
}

/**
 * eam_install_new_with_scripts:
 * @appid: the application ID to install.
 * @scriptdir: the path to the directory containing the installation scripts
 * @rollbackdir: the path to the directory containing the rollback scripts
 *
 * Returns: a new instance of #EamInstall that will run the scripts in
 * @scriptdir (and @rollbackdir if something goes wrong).
 */
EamInstall *
eam_install_new_with_scripts (const gchar *appid, const gchar *scriptdir, const gchar *rollbackdir)
{
  return g_object_new (EAM_TYPE_INSTALL, "appid", appid,
    "scriptdir", scriptdir, "rollback-scriptdir", rollbackdir, NULL);
}

/**
 * eam_install_run_async:
 * @install: a #GType supporting #EamInstall.
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the resquest is satisfied.
 * @data: (closure): the data to pass to callback function.
 *
 * Run the main method of the installation.
 **/
void
eam_install_run_async (EamInstall *install, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer data)
{
  eam_install_transaction_run_async (EAM_TRANSACTION (install), cancellable, callback, data);
}

/**
 * eam_install_finish:
 * @trans: a #GType supporting #EamInstall.
 * @res: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Finishes the installation.
 *
 * Returns: %TRUE if everything went OK.
 **/
gboolean
eam_install_finish (EamInstall *install, GAsyncResult *res, GError **error)
{
  return eam_install_transaction_finish (EAM_TRANSACTION (install), res, error);
}

static gchar *
build_tarball_filename (const gchar *appid)
{
  gchar *fname = g_strconcat (appid, ".bundle", NULL);
  gchar *ret = g_build_filename (eam_config_dldir (), fname, NULL);
  g_free (fname);
  return ret;
}

static void
rollback (GTask *task)
{
  EamInstall *self = EAM_INSTALL (g_task_get_source_object (task));
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  /* scripts directory path */
  char *dir = g_build_filename (eam_config_scriptdir (), priv->rollback_scriptdir, NULL);

  /* scripts parameters */
  GStrv params = g_new0 (gchar *, 3);
  params[0] = g_strdup (priv->appid);
  params[1] = build_tarball_filename (priv->appid);

  /* prefix environment */
  g_setenv ("PREFIX", eam_config_appdir (), FALSE);
  g_setenv ("TMP", g_get_tmp_dir (), FALSE);

  GCancellable *cancellable = g_task_get_cancellable (task);
  EamSpawner *spawner = eam_spawner_new (dir, (const gchar * const *) params);
  eam_spawner_run_async (spawner, cancellable, NULL, NULL);

  g_free (dir);
  g_strfreev (params);
  g_object_unref (spawner);
}

static void
run_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GTask *task = data;
  GError *error = NULL;

  gboolean ret = eam_spawner_run_finish (EAM_SPAWNER (source), result, &error);
  if (error) {
    rollback (task);
    g_task_return_error (task, error);
    goto bail;
  }

  if (g_task_return_error_if_cancelled (task))
    goto bail;

  g_task_return_boolean (task, ret);

bail:
  g_object_unref (task);
}

static gchar *
build_sha256sum_filename (const gchar *appid)
{
  gchar *fname = g_strconcat (appid, ".sha256", NULL);
  gchar *ret = g_build_filename (eam_config_dldir (), fname, NULL);
  g_free (fname);
  return ret;
}

static gchar *
build_sha256sum_contents (const gchar *hash, const gchar *fname)
{
  return g_strconcat (hash, "\t", fname, "\n", NULL);
}

static void
dl_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GTask *task = data;
  GError *error = NULL;

  eam_wc_request_finish (EAM_WC (source), result, &error);
  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  if (g_task_return_error_if_cancelled (task))
    goto bail;

  EamInstall *self = EAM_INSTALL (g_task_get_source_object (task));
  EamInstallPrivate *priv = eam_install_get_instance_private (self);
  g_assert (priv->hash);

  gchar *tarball = build_tarball_filename (priv->appid);

  /* let's create check-sha256sum file */
  {
    gchar *fn = build_sha256sum_filename (priv->appid);
    gchar *contents = build_sha256sum_contents (priv->hash, tarball);

    g_file_set_contents (fn, contents, -1, &error);

    g_free (fn);
    g_free (contents);
  }
  if (error) {
    g_free (tarball);
    g_task_return_error (task, error);
    goto bail;
  }

  /* scripts directory path */
  char *dir = g_build_filename (eam_config_scriptdir (), priv->scriptdir, NULL);

  /* scripts parameters */
  GStrv params = g_new0 (gchar *, 3);
  params[0] = g_strdup (priv->appid);
  params[1] = tarball;

  /* prefix environment */
  g_setenv ("PREFIX", eam_config_appdir (), FALSE);
  g_setenv ("TMP", g_get_tmp_dir (), FALSE);

  EamSpawner *spawner = eam_spawner_new (dir, (const gchar * const *) params);
  GCancellable *cancellable = g_task_get_cancellable (task);
  eam_spawner_run_async (spawner, cancellable, run_cb, task);

  g_free (dir);
  g_strfreev (params);
  g_object_unref (spawner);

  return;

bail:
  g_object_unref (task);
}

static JsonObject *
find_pkg_version (JsonArray *array, const gchar *version)
{
  GList *el = json_array_get_elements (array);
  if (!el)
    return NULL;

  GList *l;
  JsonObject *max = NULL;
  for (l = el; l; l = l->next) {
    JsonNode *node = l->data;

    if (!JSON_NODE_HOLDS_OBJECT (node))
      continue;

    JsonObject *json = json_node_get_object (node);
    const gchar *verstr = json_object_get_string_member (json, "codeVersion");
    if (!verstr)
      continue;

    /* do we have a version and does this json have this very version
     * to install? */
    if (version && !g_strcmp0 (verstr, version)) {
      max = json;
      goto bail;
    }

    /* the fist element is our local max in the first iteration */
    if (!max) {
      max = json;
      continue;
    }

    EamPkgVersion *curver = eam_pkg_version_new_from_string (verstr);
    EamPkgVersion *maxver = eam_pkg_version_new_from_string (
      json_object_get_string_member (max, "codeVersion"));

    /* if curver > maxver -> maxjson = curjson */
    if (eam_pkg_version_relate (curver, EAM_RELATION_GT, maxver))
      max = json;

    eam_pkg_version_free (curver);
    eam_pkg_version_free (maxver);
  }

bail:
  g_list_free (el);
  return max;
}

/**
 * expected json format:
 *
 * {
 *   "appId": "com.application.id2",
 *   "appName": "App Name 2",
 *   "codeVersion": "2.22",
 *   "minOsVersion": "1eos1",
 *   "downloadLink": "http://twourl.com/220to222",
 *   "shaHash": "bbccddee-2.20",
 *   "isDiff": true,
 *   "fromVersion": "2.20"
 * }
 **/
static void
parse_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GTask *task = data;
  GError *error = NULL;
  JsonParser *parser = JSON_PARSER (source);

  json_parser_load_from_stream_finish (parser, result, &error);
  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  if (g_task_return_error_if_cancelled (task))
    goto bail;

  JsonNode *root = json_parser_get_root (parser);

  EamInstall *self = EAM_INSTALL (g_task_get_source_object (task));
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  JsonObject *json = NULL;
  if (JSON_NODE_HOLDS_ARRAY (root)) {
    json = find_pkg_version (json_node_get_array (root), priv->version);
  } else if (JSON_NODE_HOLDS_OBJECT (root)) {
    json = json_node_get_object (root);
  }

  if (!json) {
    g_task_return_new_error (task, EAM_TRANSACTION_ERROR,
       EAM_TRANSACTION_ERROR_INVALID_FILE,
       _("Not valid stream with update/install link"));
    goto bail;
  }

  const gchar *path = json_object_get_string_member (json, "downloadLink");
  const gchar *hash = json_object_get_string_member (json, "shaHash");
  const gchar *version = json_object_get_string_member (json, "codeVersion");

  if (!path || !hash) {
    g_task_return_new_error (task, EAM_TRANSACTION_ERROR,
       EAM_TRANSACTION_ERROR_INVALID_FILE,
       _("Not valid application link"));
    goto bail;
  }

  gchar *uri = eam_rest_build_uri (EAM_REST_API_V1_GET_APP_DOWNLOAD_LINK, path,
    NULL);
  if (!uri) {
    g_task_return_new_error (task, EAM_TRANSACTION_ERROR,
      EAM_TRANSACTION_ERROR_PROTOCOL_ERROR,
      _("Not valid method or protocol version"));
    goto bail;
  }

  {
    g_free (priv->hash);
    priv->hash = g_strdup (hash);
    if (!priv->version)
      priv->version = g_strdup (version);
  }

  GCancellable *cancellable = g_task_get_cancellable (task);
  EamWc *wc = eam_wc_new ();
  gchar *filename = build_tarball_filename (priv->appid);
  eam_wc_request_async (wc, uri, filename, cancellable, dl_cb, task);
  g_object_unref (wc);
  g_free (filename);
  g_free (uri);
  return;

bail:
  g_object_unref (task);
}

static void
request_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GTask *task = data;
  GError *error = NULL;

  GInputStream *strm = eam_wc_request_instream_finish (EAM_WC (source),
     result, &error);
  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  if (g_task_return_error_if_cancelled (task)) {
    g_object_unref (strm);
    goto bail;
  }

  g_assert (strm);

  GCancellable *cancellable = g_task_get_cancellable (task);
  JsonParser *parser = json_parser_new ();
  json_parser_load_from_stream_async (parser, strm, cancellable, parse_cb, task);
  g_object_unref (parser);
  g_object_unref (strm);

  return;

bail:
  g_object_unref (task);
}

/**
 * 1. Download the update link
 * 1. Download the application and its sha256sum
 * 2. Set the envvars
 * 3. Run the scripts
 * 4. Rollback if something fails
 */
static void
eam_install_transaction_run_async (EamTransaction *trans, GCancellable *cancellable,
  GAsyncReadyCallback callback, gpointer data)
{
  g_return_if_fail (EAM_IS_INSTALL (trans));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  /* is it enough? */
  if (!g_network_monitor_get_network_available (g_network_monitor_get_default ())) {
    g_task_report_new_error (trans, callback, data, eam_install_run_async,
      EAM_TRANSACTION_ERROR, EAM_TRANSACTION_ERROR_NO_NETWORK,
      _("Networking is not available"));
    return;
  }

  EamInstall *self = EAM_INSTALL (trans);
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  gchar *uri = eam_rest_build_uri (EAM_REST_API_V1_GET_APP_UPDATE_LINK,
    priv->appid, priv->version, NULL);

  if (!uri) {
    g_task_report_new_error (self, callback, data, eam_install_run_async,
      EAM_TRANSACTION_ERROR, EAM_TRANSACTION_ERROR_PROTOCOL_ERROR,
      _("Not valid method or protocol version"));
    return;
  }

  GTask *task = g_task_new (self, cancellable, callback, data);

  EamWc *wc = eam_wc_new ();
  eam_wc_request_instream_with_headers_async (wc, uri, cancellable, request_cb,
    task, "Accept", "application/json", "personality",
    eam_os_get_personality (), NULL);

  g_free (uri);
  g_object_unref (wc);
}

static gboolean
eam_install_transaction_finish (EamTransaction *trans, GAsyncResult *res, GError **error)
{
  g_return_val_if_fail (EAM_IS_INSTALL (trans), FALSE);
  g_return_val_if_fail (g_task_is_valid (res, trans), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}
