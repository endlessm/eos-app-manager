/* Copyright 2014 Endless Mobile, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "eam-update.h"
#include "eam-install.h"
#include "eam-rest.h"
#include "eam-spawner.h"
#include "eam-wc.h"
#include "eam-config.h"
#include "eam-os.h"
#include "eam-version.h"

#define XDELTA_UPDATE_SCRIPTDIR "update/xdelta"
#define ROLLBACK_SCRIPTDIR "update/rollback"


typedef struct _EamUpdatePrivate	EamUpdatePrivate;

struct _EamUpdatePrivate
{
  gchar *appid;
  gchar *from_version;
  gchar *hash;
  gchar *signature_url;
};

static void transaction_iface_init (EamTransactionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EamUpdate, eam_update, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (EAM_TYPE_TRANSACTION, transaction_iface_init)
  G_ADD_PRIVATE (EamUpdate));

enum
{
  PROP_APPID = 1,
  PROP_FROM_VERSION,
};

static inline void
run_scripts (EamUpdate *self, const gchar *scriptdir, gchar *tarball,
  GAsyncReadyCallback callback, GTask *task)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  /* scripts directory path */
  char *dir = g_build_filename (eam_config_scriptdir (), scriptdir, NULL);

  /* scripts parameters */
  GStrv params = g_new0 (gchar *, 3);
  params[0] = g_strdup (priv->appid);
  params[1] = tarball;

  /* prefix environment */
  g_setenv ("EAM_PREFIX", eam_config_appdir (), FALSE);
  g_setenv ("EAM_TMP", g_get_tmp_dir (), FALSE);
  g_setenv ("EAM_GPGKEYRING", eam_config_gpgkeyring (), FALSE);

  EamSpawner *spawner = eam_spawner_new (dir, (const gchar * const *) params);
  GCancellable *cancellable = g_task_get_cancellable (task);
  eam_spawner_run_async (spawner, cancellable, callback, task);

  g_free (dir);
  g_strfreev (params);
  g_object_unref (spawner);
}


static void
fallback_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  g_return_if_fail (EAM_IS_INSTALL (source));

  GTask *task = data;
  GError *error = NULL;

  gboolean ret = eam_install_finish (EAM_INSTALL (source), result, &error);
  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  if (g_task_return_error_if_cancelled (task))
    goto bail;

  g_task_return_boolean (task, ret);

bail:
  g_object_unref (source);
  g_object_unref (task);
}

static void
rollback_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GTask *task = data;
  EamUpdate *self = EAM_UPDATE (g_task_get_source_object (task));
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);
  GCancellable *cancellable = g_task_get_cancellable (task);

  EamInstall *install = eam_install_new_update (priv->appid);
  eam_install_run_async (install, cancellable, fallback_cb, task);
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
  EamUpdate *self = EAM_UPDATE (g_task_get_source_object (task));
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);
  gchar *tarball = build_tarball_filename (priv->appid);

  run_scripts (self, ROLLBACK_SCRIPTDIR, tarball, rollback_cb, task);
}

static void
run_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GTask *task = data;
  GError *error = NULL;

  gboolean ret = eam_spawner_run_finish (EAM_SPAWNER (source), result, &error);
  if (error) {
    rollback (task);
    g_warning ("Incremental update failed: %s", error->message);
    g_clear_error (&error);
    return;
  }

  if (g_task_return_error_if_cancelled (task))
    goto bail;

  g_task_return_boolean (task, ret);

bail:
  g_object_unref (task);
}

static inline gchar *
build_sha256sum_filename (const gchar *appid)
{
  gchar *fname = g_strconcat (appid, ".sha256", NULL);
  gchar *ret = g_build_filename (eam_config_dldir (), fname, NULL);
  g_free (fname);
  return ret;
}

static inline void
create_sha256sum_file (EamUpdate *self, const gchar *tarball, GError **error)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);
  g_assert (priv->hash);

  gchar *filename = build_sha256sum_filename (priv->appid);
  gchar *contents = g_strconcat (priv->hash, "\t", tarball, "\n", NULL);

  g_file_set_contents (filename, contents, -1, error);

  g_free (filename);
  g_free (contents);
}

static void
dl_sign_cb (GObject *source, GAsyncResult *result, gpointer data)
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

  EamUpdate *self = EAM_UPDATE (g_task_get_source_object (task));
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);
  gchar *tarball = build_tarball_filename (priv->appid);

  create_sha256sum_file (self, tarball, &error);
  if (error) {
    g_free (tarball);
    g_task_return_error (task, error);
    goto bail;
  }

  run_scripts (self, XDELTA_UPDATE_SCRIPTDIR, tarball, run_cb, task);

  return;

bail:
  g_object_unref (task);
}

static gchar *
build_sign_filename (const gchar *appid)
{
  gchar *fname = g_strconcat (appid, ".asc", NULL);
  gchar *ret = g_build_filename (eam_config_dldir (), fname, NULL);
  g_free (fname);
  return ret;
}

static void
dl_bundle_cb (GObject *source, GAsyncResult *result, gpointer data)
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

  /* download signature */
  /* @TODO: make all downloads in parallel */
  EamUpdate *self = EAM_UPDATE (g_task_get_source_object (task));
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);
  gchar *filename = build_sign_filename (priv->appid);

  GCancellable *cancellable = g_task_get_cancellable (task);
  EamWc *wc = eam_wc_new ();
  eam_wc_request_async (wc, priv->signature_url, filename, cancellable,
    dl_sign_cb, task);
  g_object_unref (wc);
  g_free (filename);

  return;

bail:
  g_object_unref (task);
}

static JsonObject *
find_pkg_version (JsonArray *array, const gchar *from_version)
{
  GList *el = json_array_get_elements (array);
  if (!el)
    return NULL;

  EamPkgVersion *fromver = eam_pkg_version_new_from_string (from_version);

  GList *l;
  JsonObject *max = NULL;
  for (l = el; l; l = l->next) {
    JsonNode *node = l->data;

    if (!JSON_NODE_HOLDS_OBJECT (node))
      continue;

    JsonObject *json = json_node_get_object (node);

    /* Check 'is diff' */
    gboolean is_diff = json_object_get_boolean_member (json, "isDiff");
    if (!is_diff)
      continue;

    /* Check 'from version' */
    const gchar *from_verstr = json_object_get_string_member (json, "fromVersion");
    if (!from_verstr)
      continue;

    EamPkgVersion *json_fromver = eam_pkg_version_new_from_string (from_verstr);
    gboolean ok_fromver = eam_pkg_version_relate (fromver, EAM_RELATION_EQ, json_fromver);
    eam_pkg_version_free (json_fromver);

    if (!ok_fromver)
      continue;

    /* Get the maximun 'code version' */
    const gchar *verstr = json_object_get_string_member (json, "codeVersion");
    if (!verstr)
      continue;

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

  g_list_free (el);
  eam_pkg_version_free (fromver);
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

  EamUpdate *self = EAM_UPDATE (g_task_get_source_object (task));
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  JsonObject *json = NULL;
  if (JSON_NODE_HOLDS_ARRAY (root)) {
    json = find_pkg_version (json_node_get_array (root), priv->from_version);
  } else if (JSON_NODE_HOLDS_OBJECT (root)) {
    json = json_node_get_object (root);
  }

  if (!json) {
    /* No xdelta updates available. Try the full update */
    GCancellable *cancellable = g_task_get_cancellable (task);
    EamInstall *install = eam_install_new_update (priv->appid);
    eam_install_run_async (install, cancellable, fallback_cb, task);

    return;
  }

  const gchar *uri = json_object_get_string_member (json, "downloadLink");
  const gchar *sign = json_object_get_string_member (json, "signatureLink");
  const gchar *hash = json_object_get_string_member (json, "shaHash");

  if (!uri || !hash) {
    g_task_return_new_error (task, EAM_TRANSACTION_ERROR,
       EAM_TRANSACTION_ERROR_INVALID_FILE,
       _("Not valid application link"));
    goto bail;
  }

  if (!sign) {
    g_task_return_new_error (task, EAM_TRANSACTION_ERROR,
      EAM_TRANSACTION_ERROR_INVALID_FILE,
      _("Not valid signature link"));
    goto bail;
  }

  {
    g_free (priv->hash);
    priv->hash = g_strdup (hash);
    priv->signature_url = g_strdup (sign);
  }

  GCancellable *cancellable = g_task_get_cancellable (task);
  EamWc *wc = eam_wc_new ();
  gchar *filename = build_tarball_filename (priv->appid);
  eam_wc_request_async (wc, uri, filename, cancellable, dl_bundle_cb, task);
  g_object_unref (wc);
  g_free (filename);
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
 * eam_update_run_async:
 * @trans: a #EamUpdate instance with #EamTransaction interface.
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore
 * @callback: (scope async): callback to call when the request is satisfied
 * @data: (closure): the data to pass to callback function
 *
 * Updates an application.
 */
static void
eam_update_run_async (EamTransaction *trans, GCancellable *cancellable,
  GAsyncReadyCallback callback, gpointer data)
{
  g_return_if_fail (EAM_IS_UPDATE (trans));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  /* is it enough? */
  if (!g_network_monitor_get_network_available (g_network_monitor_get_default ())) {
    g_task_report_new_error (trans, callback, data, eam_update_run_async,
      EAM_TRANSACTION_ERROR, EAM_TRANSACTION_ERROR_NO_NETWORK,
      _("Networking is not available"));
    return;
  }

  EamUpdate *self = EAM_UPDATE (trans);
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  gchar *uri = eam_rest_build_uri (EAM_REST_API_V1_GET_APP_UPDATE_LINK,
    priv->appid, NULL, NULL);

  if (!uri) {
    g_task_report_new_error (self, callback, data, eam_update_run_async,
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

/*
 * eam_update_finish:
 * @trans:A #EamUpdate instance with #EamTransaction interface.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Finishes the update transaction.
 *
 * Returns: %TRUE if everything went well, %FALSE otherwise
 **/
static gboolean
eam_update_finish (EamTransaction *trans, GAsyncResult *res, GError **error)
{
  g_return_val_if_fail (EAM_IS_UPDATE (trans), FALSE);
  g_return_val_if_fail (g_task_is_valid (res, trans), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}

static void
transaction_iface_init (EamTransactionInterface *iface)
{
  iface->run_async = eam_update_run_async;
  iface->finish = eam_update_finish;
}

static void
eam_update_finalize (GObject *obj)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (EAM_UPDATE (obj));

  g_clear_pointer (&priv->appid, g_free);
  g_clear_pointer (&priv->from_version, g_free);
  g_clear_pointer (&priv->hash, g_free);
  g_clear_pointer (&priv->signature_url, g_free);

  G_OBJECT_CLASS (eam_update_parent_class)->finalize (obj);
}

static void
eam_update_get_property (GObject *obj, guint prop_id, GValue *value,
  GParamSpec *pspec)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (EAM_UPDATE (obj));

  switch (prop_id) {
  case PROP_APPID:
    g_value_set_string (value, priv->appid);
    break;
  case PROP_FROM_VERSION:
    g_value_set_string (value, priv->from_version);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
eam_update_set_property (GObject *obj, guint prop_id, const GValue *value,
  GParamSpec *pspec)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (EAM_UPDATE (obj));

  switch (prop_id) {
  case PROP_APPID:
    priv->appid = g_value_dup_string (value);
    break;
  case PROP_FROM_VERSION:
    priv->from_version = g_value_dup_string (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
eam_update_class_init (EamUpdateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eam_update_finalize;
  object_class->get_property = eam_update_get_property;
  object_class->set_property = eam_update_set_property;

  /**
   * EamUpdate:appid:
   *
   * The application ID to update.
   */
  g_object_class_install_property (object_class, PROP_APPID,
    g_param_spec_string ("appid", "App ID", "Application ID", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * EamUpdate:from-version:
   *
   * The application's version to update from.
   */
  g_object_class_install_property (object_class, PROP_FROM_VERSION,
    g_param_spec_string ("from-version", "App's version to update from",
      "Application's version to update from", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
eam_update_init (EamUpdate *self)
{
}

/**
 * eam_update_new:
 * @appid: the application ID to update.
 * @from_version: the application's version to update from.
 *
 * Returns: a new instance of #EamUpdate with #EamTransaction interface.
 */
EamTransaction *
eam_update_new (const gchar *appid, const gchar *from_version)
{
  return g_object_new (EAM_TYPE_UPDATE, "appid", appid,
    "from-version", from_version, NULL);
}
