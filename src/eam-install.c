/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "eam-log.h"
#include "eam-error.h"
#include "eam-install.h"
#include "eam-rest.h"
#include "eam-spawner.h"
#include "eam-wc.h"
#include "eam-config.h"
#include "eam-os.h"
#include "eam-version.h"
#include "eam-utils.h"

#define INSTALL_SCRIPTDIR "install/run"
#define INSTALL_ROLLBACKDIR "install/rollback"

#define INSTALL_BUNDLE_EXT ".bundle"

typedef struct _EamBundle        EamBundle;

struct _EamBundle
{
  gchar *download_url;
  gchar *signature_url;
  gchar *hash;
};

typedef struct _EamInstallPrivate        EamInstallPrivate;

struct _EamInstallPrivate
{
  gchar *appid;
  gchar *from_version;
  EamBundle *bundle;
  char *bundle_location;

  EamPkgdb *pkgdb;
  EamUpdates *updates;
};

static void initable_iface_init (GInitableIface *iface);
static void transaction_iface_init (EamTransactionInterface *iface);
static void eam_install_run_async (EamTransaction *trans, GCancellable *cancellable,
   GAsyncReadyCallback callback, gpointer data);
static gboolean eam_install_finish (EamTransaction *trans, GAsyncResult *res,
   GError **error);

G_DEFINE_TYPE_WITH_CODE (EamInstall, eam_install, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
  G_IMPLEMENT_INTERFACE (EAM_TYPE_TRANSACTION, transaction_iface_init)
  G_ADD_PRIVATE (EamInstall));

enum
{
  PROP_APPID = 1,
  PROP_PKGDB,
  PROP_UPDATES,
};

static void
transaction_iface_init (EamTransactionInterface *iface)
{
  iface->run_async = eam_install_run_async;
  iface->finish = eam_install_finish;
}

static EamBundle *
eam_bundle_new_from_json_object (JsonObject *json, GError **error)
{
  g_return_val_if_fail (json, NULL);

  const gchar *downl = json_object_get_string_member (json, "downloadLink");
  const gchar *sign = json_object_get_string_member (json, "signatureLink");
  const gchar *hash = json_object_get_string_member (json, "shaHash");

  if (!downl || !hash) {
    g_set_error (error, EAM_ERROR, EAM_ERROR_INVALID_FILE,
       _("Not valid application link"));
    return NULL;
  }

  if (!sign) {
    g_set_error (error, EAM_ERROR, EAM_ERROR_INVALID_FILE,
       _("Not valid signature link"));
    return NULL;
  }

  EamBundle *bundle = g_slice_new0 (EamBundle);
  bundle->download_url = g_strdup (downl);
  bundle->signature_url = g_strdup (sign);
  bundle->hash = g_strdup (hash);

  return bundle;
}

static void
eam_bundle_free (EamBundle *bundle)
{
  g_free (bundle->download_url);
  g_free (bundle->signature_url);
  g_free (bundle->hash);

  g_slice_free (EamBundle, bundle);
}

static void
eam_install_finalize (GObject *obj)
{
  EamInstall *self = EAM_INSTALL (obj);
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  g_clear_pointer (&priv->appid, g_free);
  g_clear_pointer (&priv->bundle_location, g_free);
  g_clear_pointer (&priv->bundle, eam_bundle_free);

  g_clear_object (&priv->updates);
  g_clear_object (&priv->pkgdb);

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
  case PROP_PKGDB:
    priv->pkgdb = g_value_dup_object (value);
    break;
  case PROP_UPDATES:
    priv->updates = g_value_dup_object (value);
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
   * The application ID to install or update.
   */
  g_object_class_install_property (object_class, PROP_APPID,
    g_param_spec_string ("appid", "App ID", "Application ID", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * EamInstall:pkgdb:
   *
   * The package database.
   */
  g_object_class_install_property (object_class, PROP_PKGDB,
   g_param_spec_object ("pkgdb", "Package DB", "Package DB", EAM_TYPE_PKGDB,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_PRIVATE));

  /**
   * EamInstall:updates:
   *
   * The updates manager.
   */
  g_object_class_install_property (object_class, PROP_UPDATES,
   g_param_spec_object ("updates", "Updates manager", "Updates manager", EAM_TYPE_UPDATES,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_PRIVATE));
}

static gboolean
eam_install_initable_init (GInitable *initable,
                           GCancellable *cancellable,
                           GError **error)
{
  EamInstall *install = EAM_INSTALL (initable);
  EamInstallPrivate *priv = eam_install_get_instance_private (install);

  if (!eam_updates_pkg_is_installable (priv->updates, priv->appid)) {
    g_set_error (error, EAM_ERROR,
                 EAM_ERROR_PKG_UNKNOWN,
                 _("Application '%s' is unknown"),
                 priv->appid);
    return FALSE;
  }

  return TRUE;
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = eam_install_initable_init;
}

static void
eam_install_init (EamInstall *self)
{
}

/**
 * eam_install_new:
 * @pkgdb: an #EamPkgdb
 * @appid: the application ID to install.
 * @updates: an #EamUpdates
 * @error: location to store a #GError
 *
 * Returns: a new instance of #EamInstall with #EamTransaction interface.
 */
EamTransaction *
eam_install_new (EamPkgdb *pkgdb,
                 const gchar *appid,
                 EamUpdates *updates,
                 GError **error)
{
  return g_initable_new (EAM_TYPE_INSTALL,
                         NULL, error,
                         "pkgdb", pkgdb,
                         "appid", appid,
                         "updates", updates,
                         NULL);
}

static gchar *
build_tarball_filename (EamInstall *self)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  return eam_utils_build_tarball_filename (priv->bundle_location, priv->appid,
                                           INSTALL_BUNDLE_EXT);
}

static const gchar *
get_rollback_scriptdir (EamInstall *self)
{
  return INSTALL_ROLLBACKDIR;
}

static const gchar *
get_scriptdir (EamInstall *self)
{
  return INSTALL_SCRIPTDIR;
}

static void
run_scripts (EamInstall *self, const gchar *scriptdir,
  GCancellable *cancellable, GAsyncReadyCallback callback, GTask *task)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  eam_utils_run_bundle_scripts (priv->appid,
                                build_tarball_filename (self),
                                scriptdir,
                                priv->bundle_location != NULL, /* external download? */
                                cancellable,
                                callback,
                                task);
}

static void
rollback (GTask *task, GError *error)
{
  EamInstall *self = EAM_INSTALL (g_task_get_source_object (task));

  /* Log some messages about what happened */
  eam_log_error_message ("Install failed: %s", error->message);

  /* Rollback action if possible */
  /* Use cancellable == NULL as we are returning immediately after
     using g_task_return. */
  run_scripts (self, get_rollback_scriptdir (self), NULL, NULL, NULL);

  /* Clean up */
  g_task_return_error (task, error);
  g_object_unref (task);
}

static void
action_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GTask *task = data;
  GError *error = NULL;

  gboolean ret = eam_spawner_run_finish (EAM_SPAWNER (source), result, &error);
  if (error) {
    rollback (task, error);
    return;
  }

  if (g_task_return_error_if_cancelled (task))
    goto bail;

  g_task_return_boolean (task, ret);

bail:
  g_object_unref (task);
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

  EamInstall *self = EAM_INSTALL (g_task_get_source_object (task));
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  gchar *tarball = build_tarball_filename (self);

  eam_utils_create_bundle_hash_file (priv->bundle->hash, tarball,
                                     priv->bundle_location,
                                     priv->appid,
                                     &error);

  g_free (tarball);

  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  GCancellable *cancellable = g_task_get_cancellable (task);
  run_scripts (self, get_scriptdir (self), cancellable, action_cb, task);

  return;

bail:
  g_object_unref (task);
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

  EamInstall *self = EAM_INSTALL (g_task_get_source_object (task));
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  eam_utils_download_bundle_signature (task, dl_sign_cb,
                                       priv->bundle->signature_url,
                                       priv->bundle_location,
                                       priv->appid);

  return;

bail:
  g_object_unref (task);
}

static void
parse_json (EamInstall *self, JsonNode *root,
  JsonObject **bundle_json)
{
  JsonObject *json = NULL;
  gboolean is_diff;
  const gchar *appid;

  *bundle_json = NULL;

  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  if (JSON_NODE_HOLDS_OBJECT (root)) {
    json = json_node_get_object (root);

    appid = json_object_get_string_member (json, "appId");
    if (!appid || g_strcmp0 (appid, priv->appid) != 0)
      return;

    is_diff = json_object_get_boolean_member (json, "isDiff");
    if (!is_diff)
      *bundle_json = json;

    return;
  }

  if (!JSON_NODE_HOLDS_ARRAY (root))
    return;

  GList *el = json_array_get_elements (json_node_get_array (root));
  if (!el)
    return;

  GList *l;
  for (l = el; l; l = l->next) {
      JsonNode *node = l->data;
      if (!JSON_NODE_HOLDS_OBJECT (node))
        continue;

      json = json_node_get_object (node);

      appid = json_object_get_string_member (json, "appId");
      if (!appid || g_strcmp0 (appid, priv->appid) != 0)
        continue;

      is_diff = json_object_get_boolean_member (json, "isDiff");
      if (is_diff)
        continue;

      if (eam_utils_compare_bundle_json_version (json, *bundle_json) > 0)
        *bundle_json = json;
  }
    g_list_free (el);
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
static gboolean
load_bundle_info (EamInstall *self, JsonNode *root)
{
  JsonObject *bundle_json = NULL;

  parse_json (self, root, &bundle_json);

  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  if (bundle_json)
    priv->bundle = eam_bundle_new_from_json_object (bundle_json, NULL);

  return priv->bundle != NULL;
}

static void
load_json_updates_cb (GObject *source, GAsyncResult *result, gpointer data)
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

  EamInstall *self = EAM_INSTALL (g_task_get_source_object (task));

  if (!load_bundle_info (self, json_parser_get_root (parser))) {
    g_task_return_new_error (task, EAM_ERROR,
       EAM_ERROR_INVALID_FILE,
       _("Not valid stream with update/install link"));
    goto bail;
  }

  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  eam_utils_download_bundle (task, dl_bundle_cb, priv->bundle->download_url,
                             priv->bundle_location,
                             priv->appid,
                             INSTALL_BUNDLE_EXT);
  return;

bail:
  g_object_unref (task);
}

static void
request_json_updates_cb (GObject *source, GAsyncResult *result, gpointer data)
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
  json_parser_load_from_stream_async (parser, strm, cancellable, load_json_updates_cb, task);
  g_object_unref (parser);
  g_object_unref (strm);

  return;

bail:
  g_object_unref (task);
}

static void
request_json_updates (EamInstall *self, GTask *task)
{
  /* is it enough? */
  if (!g_network_monitor_get_network_available (g_network_monitor_get_default ())) {
    g_task_return_new_error (task, EAM_ERROR,
      EAM_ERROR_NO_NETWORK, _("Networking is not available"));
    goto bail;
  }

  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  gchar *uri = eam_rest_build_uri (EAM_REST_API_V1_GET_APP_UPDATE_LINK,
    priv->appid, NULL, NULL);
  if (!uri) {
    g_task_return_new_error (task, EAM_ERROR,
      EAM_ERROR_PROTOCOL_ERROR, _("Not valid method or protocol version"));
    goto bail;
  }

  EamWc *wc = eam_wc_new ();
  GCancellable *cancellable = g_task_get_cancellable (task);
  eam_wc_request_instream_with_headers_async (wc, uri, cancellable, request_json_updates_cb,
    task, "Accept", "application/json", "personality",
    eam_os_get_personality (), NULL);

  g_free (uri);
  g_object_unref (wc);

  return;
bail:
  g_object_unref (task);
}

static gchar *
build_json_updates_filename (void)
{
 return g_build_filename (eam_config_dldir (), "updates.json", NULL);
}

/**
 * 1. Download the update link
 * 1. Download the application and its sha256sum
 * 2. Set the envvars
 * 3. Run the scripts
 * 4. Rollback if something fails
 */
static void
eam_install_run_async (EamTransaction *trans, GCancellable *cancellable,
  GAsyncReadyCallback callback, gpointer data)
{
  g_return_if_fail (EAM_IS_INSTALL (trans));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  JsonParser *parser = json_parser_new ();
  gchar *updates = build_json_updates_filename ();
  GError *error = NULL;

  json_parser_load_from_file (parser, updates, &error);
  g_free (updates);

  EamInstall *self = EAM_INSTALL (trans);
  GTask *task = g_task_new (self, cancellable, callback, data);

  if (error) {
    eam_log_error_message ("Can't load cached updates.json: %s", error->message);
    g_clear_error (&error);

    request_json_updates (self, task);
    goto bail;
  }

  if (!load_bundle_info (self, json_parser_get_root (parser))) {
    g_task_return_new_error (task, EAM_ERROR,
       EAM_ERROR_INVALID_FILE,
       _("Not valid stream with update/install link"));

    g_object_unref (task);
    goto bail;
  }

  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  if (priv->bundle_location == NULL) {
    eam_utils_download_bundle (task, dl_bundle_cb, priv->bundle->download_url,
                               NULL, /* bundle_location */
                               priv->appid,
                               INSTALL_BUNDLE_EXT);
  }
  else {
    run_scripts (self, get_scriptdir (self), cancellable, action_cb, task);
  }

bail:
  g_object_unref (parser);
}

static gboolean
eam_install_finish (EamTransaction *trans, GAsyncResult *res, GError **error)
{
  g_return_val_if_fail (EAM_IS_INSTALL (trans), FALSE);
  g_return_val_if_fail (g_task_is_valid (res, trans), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}

static gboolean
ensure_bundle_loaded (EamInstall *install)
{
  JsonParser *parser = json_parser_new ();
  gchar *updates = build_json_updates_filename ();
  GError *error = NULL;

  json_parser_load_from_file (parser, updates, &error);
  g_free (updates);

  if (error) {
    eam_log_error_message ("Can't load cached updates.json: %s", error->message);
    g_clear_error (&error);
    g_object_unref (parser);
    return FALSE;
  }

  if (!load_bundle_info (install, json_parser_get_root (parser))) {
    g_object_unref (parser);
    return FALSE;
  }

  g_object_unref (parser);
  return TRUE;
}

const char *
eam_install_get_bundle_hash (EamInstall *install)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (install);

  if (!ensure_bundle_loaded (install))
    return NULL;

  return priv->bundle->hash;
}

const char *
eam_install_get_signature_url (EamInstall *install)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (install);

  if (!ensure_bundle_loaded (install))
    return NULL;

  return priv->bundle->signature_url;
}

const char *
eam_install_get_download_url (EamInstall *install)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (install);

  if (!ensure_bundle_loaded (install))
    return NULL;

  return priv->bundle->download_url;
}

void
eam_install_set_bundle_location (EamInstall *install,
                                 const char *path)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (install);

  g_free (priv->bundle_location);
  priv->bundle_location = g_strdup (path);
}

const char *
eam_install_get_app_id (EamInstall *install)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (install);

  return priv->appid;
}

const gboolean
eam_install_is_delta_update (EamInstall *install)
{
  /* New install is always treated as a non-delta one */
  return FALSE;
}
