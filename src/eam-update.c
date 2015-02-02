/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "eam-log.h"
#include "eam-error.h"
#include "eam-update.h"
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

static const char *
get_extension (EamUpdate *self)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  if (priv->action == EAM_ACTION_XDELTA_UPDATE)
    return UPDATE_BUNDLE_EXT;

  return INSTALL_BUNDLE_EXT;
}

static gchar *
build_tarball_filename (EamUpdate *self)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  return eam_utils_build_tarball_filename (priv->bundle_location, priv->appid,
                                           get_extension (self));
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

  EamUpdate *self = EAM_UPDATE (g_task_get_source_object (task));
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  const gchar *hash;
  if (priv->action == EAM_ACTION_XDELTA_UPDATE)
    hash = priv->xdelta_bundle->hash;
  else
    hash = priv->bundle->hash;

  eam_wc_request_finish (EAM_WC (source), result, &error);
  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  if (g_task_return_error_if_cancelled (task))
    goto bail;

  gchar *tarball = build_tarball_filename (self);

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

  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  eam_utils_parse_json_with_updates (root,
                                     &bundle_json,
                                     &xdelta_json,
                                     priv->appid,
                                     priv->from_version,
                                     priv->action == EAM_ACTION_UPDATE); /* ignore deltas */

  if (priv->action == EAM_ACTION_XDELTA_UPDATE) {
    if (xdelta_json && bundle_json) {
      /* If they have different versions, discard the oldest one.
         The AppManager always updates to the newest version available. */
      gint result = eam_utils_compare_bundle_json_version (bundle_json, xdelta_json);
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
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  if (!load_bundle_info (self, json_parser_get_root (parser))) {
    g_task_return_new_error (task, EAM_ERROR,
       EAM_ERROR_INVALID_FILE,
       _("Not valid stream with update link"));
    goto bail;
  }

  eam_utils_download_bundle (task, dl_bundle_cb, priv->bundle->download_url,
                             priv->bundle_location,
                             priv->appid,
                             get_extension (self));
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

  GError *error = NULL;

  JsonParser *parser = json_parser_new ();
  gchar *updates = eam_utils_get_json_updates_filename ();

  json_parser_load_from_file (parser, updates, &error);
  g_free (updates);

  EamUpdate *self = EAM_UPDATE (trans);
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  GTask *task = g_task_new (self, cancellable, callback, data);

  if (error) {
    eam_log_error_message ("Can't load cached updates.json: %s", error->message);
    g_clear_error (&error);

    eam_utils_request_json_updates (task, request_json_updates_cb, priv->appid);

    goto bail;
  }

  if (!load_bundle_info (self, json_parser_get_root (parser))) {
    g_task_return_new_error (task, EAM_ERROR,
       EAM_ERROR_INVALID_FILE,
       _("Not valid stream with update link"));

    g_object_unref (task);
    goto bail;
  }

  if (priv->bundle_location == NULL) {
    eam_utils_download_bundle (task, dl_bundle_cb, priv->bundle->download_url,
                               NULL, /* bundle_location */
                               priv->appid,
                               get_extension (self));
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
  GError *error = NULL;

  JsonParser *parser = json_parser_new ();
  gchar *updates = eam_utils_get_json_updates_filename ();

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
