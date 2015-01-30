/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "eam-log.h"
#include "eam-error.h"
#include "eam-update.h"
#include "eam-rest.h"
#include "eam-spawner.h"
#include "eam-wc.h"
#include "eam-config.h"
#include "eam-os.h"
#include "eam-version.h"
#include "eam-utils.c"

#define UPDATE_SCRIPTDIR "update/full"
#define UPDATE_ROLLBACKDIR "update/rollback"
#define XDELTA_UPDATE_SCRIPTDIR "update/xdelta"
#define XDELTA_UPDATE_ROLLBACKDIR "update/rollback"

#define UPDATE_BUNDLE_EXT ".xdelta"
#define INSTALL_BUNDLE_EXT ".bundle"

typedef struct _EamBundle        EamBundle;

struct _EamBundle
{
  gchar *download_url;
  gchar *signature_url;
  gchar *hash;
};

typedef enum {
  EAM_ACTION_UPDATE,        /* Update downloading the complete bundle */
  EAM_ACTION_XDELTA_UPDATE, /* Update applying xdelta diff files */
} EamAction;

typedef struct _EamUpdatePrivate        EamUpdatePrivate;

struct _EamUpdatePrivate
{
  gchar *appid;
  gchar *from_version;
  EamAction action;
  EamBundle *bundle;
  EamBundle *xdelta_bundle;
  char *bundle_location;

  EamPkgdb *pkgdb;
  EamUpdates *updates;
};

static void initable_iface_init (GInitableIface *iface);
static void transaction_iface_init (EamTransactionInterface *iface);
static void eam_update_run_async (EamTransaction *trans, GCancellable *cancellable,
   GAsyncReadyCallback callback, gpointer data);
static gboolean eam_update_finish (EamTransaction *trans, GAsyncResult *res,
   GError **error);

G_DEFINE_TYPE_WITH_CODE (EamUpdate, eam_update, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
  G_IMPLEMENT_INTERFACE (EAM_TYPE_TRANSACTION, transaction_iface_init)
  G_ADD_PRIVATE (EamUpdate));

enum
{
  PROP_APPID = 1,
  PROP_PKGDB,
  PROP_UPDATES,
};

static void
transaction_iface_init (EamTransactionInterface *iface)
{
  iface->run_async = eam_update_run_async;
  iface->finish = eam_update_finish;
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
eam_update_finalize (GObject *obj)
{
  EamUpdate *self = EAM_UPDATE (obj);
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  g_clear_pointer (&priv->appid, g_free);
  g_clear_pointer (&priv->bundle_location, g_free);
  g_clear_pointer (&priv->bundle, eam_bundle_free);
  g_clear_pointer (&priv->xdelta_bundle, eam_bundle_free);

  g_clear_object (&priv->updates);
  g_clear_object (&priv->pkgdb);

  G_OBJECT_CLASS (eam_update_parent_class)->finalize (obj);
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
eam_update_get_property (GObject *obj, guint prop_id, GValue *value,
  GParamSpec *pspec)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (EAM_UPDATE (obj));

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
eam_update_class_init (EamUpdateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eam_update_finalize;
  object_class->get_property = eam_update_get_property;
  object_class->set_property = eam_update_set_property;

  /**
   * EamUpdate:appid:
   *
   * The application ID to update
   */
  g_object_class_install_property (object_class, PROP_APPID,
    g_param_spec_string ("appid", "App ID", "Application ID", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * EamUpdate:pkgdb:
   *
   * The package database.
   */
  g_object_class_install_property (object_class, PROP_PKGDB,
   g_param_spec_object ("pkgdb", "Package DB", "Package DB", EAM_TYPE_PKGDB,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_PRIVATE));

  /**
   * EamUpdate:updates:
   *
   * The updates manager.
   */
  g_object_class_install_property (object_class, PROP_UPDATES,
   g_param_spec_object ("updates", "Updates manager", "Updates manager", EAM_TYPE_UPDATES,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_PRIVATE));
}

static gboolean
eam_update_initable_init (GInitable *initable,
                          GCancellable *cancellable,
                          GError **error)
{
  EamUpdate *update = EAM_UPDATE (initable);
  EamUpdatePrivate *priv = eam_update_get_instance_private (update);

  if (eam_updates_pkg_is_upgradable (priv->updates, priv->appid)) {
    /* update from the current version to the last version */
    const EamPkg *pkg = eam_pkgdb_get (priv->pkgdb, priv->appid);
    g_assert (pkg);

    EamPkgVersion *pkg_version = eam_pkg_get_version (pkg);
    priv->from_version = eam_pkg_version_as_string (pkg_version);
    priv->action = eam_config_deltaupdates () ? EAM_ACTION_XDELTA_UPDATE : EAM_ACTION_UPDATE;
  }
  else {
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
  iface->init = eam_update_initable_init;
}

static void
eam_update_init (EamUpdate *self)
{
}

/**
 * eam_update_new:
 * @pkgdb: an #EamPkgdb
 * @appid: the application ID to update.
 * @allow_deltas: whether to allow incremental updates
 * @updates: an #EamUpdates
 * @error: location to store a #GError
 *
 * Returns: a new instance of #EamUpdate with #EamTransaction interface.
 */
EamTransaction *
eam_update_new (EamPkgdb *pkgdb,
                const gchar *appid,
                const gboolean allow_deltas,
                EamUpdates *updates,
                GError **error)
{
  return g_initable_new (EAM_TYPE_UPDATE,
                         NULL, error,
                         "pkgdb", pkgdb,
                         "appid", appid,
                         "updates", updates,
                         NULL);
}

static gchar *
build_tarball_filename (EamUpdate *self)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  if (priv->bundle_location != NULL)
    return g_strdup (priv->bundle_location);

  gchar *fname = NULL;

  switch (priv->action) {
  case EAM_ACTION_XDELTA_UPDATE:
    fname = g_strconcat (priv->appid, UPDATE_BUNDLE_EXT, NULL);
  default:
    fname = g_strconcat (priv->appid, INSTALL_BUNDLE_EXT, NULL);
  }

  gchar *ret = g_build_filename (eam_config_dldir (), fname, NULL);
  g_free (fname);

  return ret;
}

static const gchar *
get_rollback_scriptdir (EamUpdate *self)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  switch (priv->action) {
  case EAM_ACTION_UPDATE:
    return UPDATE_ROLLBACKDIR;
  case EAM_ACTION_XDELTA_UPDATE:
    return XDELTA_UPDATE_ROLLBACKDIR;
  default:
    g_assert_not_reached ();
  }
}

static const gchar *
get_scriptdir (EamUpdate *self)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  switch (priv->action) {
  case EAM_ACTION_UPDATE:
    return UPDATE_SCRIPTDIR;
  case EAM_ACTION_XDELTA_UPDATE:
    return XDELTA_UPDATE_SCRIPTDIR;
  default:
    g_assert_not_reached ();
  }
}

static void
run_scripts (EamUpdate *self, const gchar *scriptdir,
  GCancellable *cancellable, GAsyncReadyCallback callback, GTask *task)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

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
  EamUpdate *self = EAM_UPDATE (g_task_get_source_object (task));
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  /* Log some messages about what happened */
  switch (priv->action) {
  case EAM_ACTION_XDELTA_UPDATE:
    eam_log_error_message ("Incremental update failed: %s", error->message);
    break;
  case EAM_ACTION_UPDATE:
    eam_log_error_message ("Full update failed: %s", error->message);
    break;
  }

  /* Rollback action if possible */
  if (priv->action == EAM_ACTION_XDELTA_UPDATE && !priv->bundle) {
    eam_log_error_message ("Can't rollback xdelta update - bundle not present.");
  } else {
    /* Use cancellable == NULL as we are returning immediately after
       using g_task_return. */
    run_scripts (self, get_rollback_scriptdir (self), NULL, NULL, NULL);
  }

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

  EamUpdate *self = EAM_UPDATE (g_task_get_source_object (task));
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  gchar *tarball = build_tarball_filename (self);

  const gchar *hash;
  if (priv->action == EAM_ACTION_XDELTA_UPDATE)
    hash = priv->xdelta_bundle->hash;
  else
    hash = priv->bundle->hash;

  eam_utils_create_bundle_hash_file (hash, tarball, priv->bundle_location,
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

  EamUpdate *self = EAM_UPDATE (g_task_get_source_object (task));
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  eam_utils_download_bundle_signature (task, dl_sign_cb,
                                       priv->bundle->signature_url,
                                       priv->bundle_location,
                                       priv->appid);

  return;

bail:
  g_object_unref (task);
}

static void
download_bundle (EamUpdate *self, GTask *task)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  gchar *filename = build_tarball_filename (self);
  const gchar *download_url;
  if (priv->action == EAM_ACTION_XDELTA_UPDATE)
    download_url = priv->xdelta_bundle->download_url;
  else
    download_url = priv->bundle->download_url;

  EamWc *wc = eam_wc_new ();
  GCancellable *cancellable = g_task_get_cancellable (task);
  eam_wc_request_async (wc, download_url, filename, cancellable, dl_bundle_cb, task);

  g_object_unref (wc);
  g_free (filename);
}

static gboolean
xdelta_is_candidate (EamUpdate *self, JsonObject *json)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  if (priv->action != EAM_ACTION_XDELTA_UPDATE)
    return FALSE;

  const gchar *from_version = json_object_get_string_member (json, "fromVersion");
  if (!from_version)
    return FALSE;

  EamPkgVersion *app_pkg_from_ver = eam_pkg_version_new_from_string (priv->from_version);
  EamPkgVersion *pkg_from_ver = eam_pkg_version_new_from_string (from_version);

  gboolean equal = eam_pkg_version_relate (app_pkg_from_ver, EAM_RELATION_EQ, pkg_from_ver);

  eam_pkg_version_free (app_pkg_from_ver);
  eam_pkg_version_free (pkg_from_ver);

  return equal;
}

/* Returns: 0 if both versions are equal, <0 if a is smaller than b,
 * >0 if a is greater than b. */
static gint
compare_version_from_json (JsonObject *a, JsonObject *b)
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

static void
parse_json (EamUpdate *self, JsonNode *root,
  JsonObject **bundle_json, JsonObject **xdelta_json)
{
  JsonObject *json = NULL;
  gboolean is_diff;
  const gchar *appid;

  *xdelta_json = NULL;
  *bundle_json = NULL;

  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  if (JSON_NODE_HOLDS_OBJECT (root)) {
    json = json_node_get_object (root);

    appid = json_object_get_string_member (json, "appId");
    if (!appid || g_strcmp0 (appid, priv->appid) != 0)
      return;

    is_diff = json_object_get_boolean_member (json, "isDiff");
    if (is_diff) {
      if (xdelta_is_candidate (self, json))
        *xdelta_json = json;
    } else {
      *bundle_json = json;
    }
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
      if (is_diff) {
        if (xdelta_is_candidate (self, json)) {
          if (compare_version_from_json (json, *xdelta_json) > 0)
            *xdelta_json = json;
        }
        continue;
      }
      if (compare_version_from_json (json, *bundle_json) > 0)
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
load_bundle_info (EamUpdate *self, JsonNode *root)
{
  JsonObject *xdelta_json = NULL;
  JsonObject *bundle_json = NULL;

  parse_json (self, root, &bundle_json, &xdelta_json);

  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  if (priv->action == EAM_ACTION_XDELTA_UPDATE) {
    if (xdelta_json && bundle_json) {
      /* If they have different versions, discard the oldest one.
         The AppManager always updates to the newest version available. */
      gint result = compare_version_from_json (bundle_json, xdelta_json);
      if (result < 0)
        bundle_json = NULL;
      else if (result > 0)
        xdelta_json = NULL;
    }

    /* If there is no xdelta, do a full update */
    if (!xdelta_json)
      priv->action = EAM_ACTION_UPDATE;
  }

  switch (priv->action) {
  case EAM_ACTION_XDELTA_UPDATE:
    priv->xdelta_bundle = eam_bundle_new_from_json_object (xdelta_json, NULL);
    if (bundle_json)
      /* Used as fallback if the xdelta update fails */
      priv->bundle = eam_bundle_new_from_json_object (bundle_json, NULL);
    break;
  case EAM_ACTION_UPDATE:
    if (bundle_json)
      priv->bundle = eam_bundle_new_from_json_object (bundle_json, NULL);
    break;
  }

  return (priv->bundle || priv->xdelta_bundle);
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

  EamUpdate *self = EAM_UPDATE (g_task_get_source_object (task));

  if (!load_bundle_info (self, json_parser_get_root (parser))) {
    g_task_return_new_error (task, EAM_ERROR,
       EAM_ERROR_INVALID_FILE,
       _("Not valid stream with update link"));
    goto bail;
  }

  download_bundle (self, task);
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
request_json_updates (EamUpdate *self, GTask *task)
{
  /* is it enough? */
  if (!g_network_monitor_get_network_available (g_network_monitor_get_default ())) {
    g_task_return_new_error (task, EAM_ERROR,
      EAM_ERROR_NO_NETWORK, _("Networking is not available"));
    goto bail;
  }

  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

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
eam_update_run_async (EamTransaction *trans, GCancellable *cancellable,
  GAsyncReadyCallback callback, gpointer data)
{
  g_return_if_fail (EAM_IS_UPDATE (trans));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  JsonParser *parser = json_parser_new ();
  gchar *updates = build_json_updates_filename ();
  GError *error = NULL;

  json_parser_load_from_file (parser, updates, &error);
  g_free (updates);

  EamUpdate *self = EAM_UPDATE (trans);
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
       _("Not valid stream with update link"));

    g_object_unref (task);
    goto bail;
  }

  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  if (priv->bundle_location == NULL) {
    download_bundle (self, task);
  }
  else {
    run_scripts (self, get_scriptdir (self), cancellable, action_cb, task);
  }

bail:
  g_object_unref (parser);
}

static gboolean
eam_update_finish (EamTransaction *trans, GAsyncResult *res, GError **error)
{
  g_return_val_if_fail (EAM_IS_UPDATE (trans), FALSE);
  g_return_val_if_fail (g_task_is_valid (res, trans), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}

static gboolean
ensure_bundle_loaded (EamUpdate *update)
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

  if (!load_bundle_info (update, json_parser_get_root (parser))) {
    g_object_unref (parser);
    return FALSE;
  }

  g_object_unref (parser);
  return TRUE;
}

const char *
eam_update_get_bundle_hash (EamUpdate *update)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (update);
  const gchar *bundle_hash;

  if (!ensure_bundle_loaded (update))
    return NULL;

  if (priv->action == EAM_ACTION_XDELTA_UPDATE)
    bundle_hash = priv->xdelta_bundle->hash;
  else
    bundle_hash = priv->bundle->hash;

  return bundle_hash;
}

const char *
eam_update_get_signature_url (EamUpdate *update)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (update);
  const gchar *signature_url;

  if (!ensure_bundle_loaded (update))
    return NULL;

  if (priv->action == EAM_ACTION_XDELTA_UPDATE) {
    signature_url = priv->xdelta_bundle->signature_url;
  }
  else {
    signature_url = priv->bundle->signature_url;
  }

  return signature_url;
}

const char *
eam_update_get_download_url (EamUpdate *update)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (update);
  const gchar *download_url;

  if (!ensure_bundle_loaded (update))
    return NULL;

  if (priv->action == EAM_ACTION_XDELTA_UPDATE) {
    download_url = priv->xdelta_bundle->download_url;
  }
  else {
    download_url = priv->bundle->download_url;
  }

  return download_url;
}

void
eam_update_set_bundle_location (EamUpdate *update,
                                 const char *path)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (update);

  g_free (priv->bundle_location);
  priv->bundle_location = g_strdup (path);
}

const char *
eam_update_get_app_id (EamUpdate *update)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (update);

  return priv->appid;
}

const gboolean
eam_update_is_delta_update (EamUpdate *update)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (update);

  return (priv->action == EAM_ACTION_XDELTA_UPDATE);
}
