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

#define INSTALL_SCRIPTDIR "install/run"
#define INSTALL_ROLLBACKDIR "install/rollback"
#define UPDATE_SCRIPTDIR "update/full"
#define UPDATE_ROLLBACKDIR "update/rollback"
#define XDELTA_UPDATE_SCRIPTDIR "update/xdelta"
#define XDELTA_UPDATE_ROLLBACKDIR "update/rollback"

typedef struct _EamBundle        EamBundle;

struct _EamBundle
{
  gchar *download_url;
  gchar *signature_url;
  gchar *hash;
};

typedef enum {
  EAM_ACTION_INSTALL,       /* Install */
  EAM_ACTION_UPDATE,        /* Update downloading the complete bundle */
  EAM_ACTION_XDELTA_UPDATE, /* Update applying xdelta diff files */
} EamAction;

/* EamAction GType definition */
#define EAM_TYPE_ACTION	(eam_action_get_type())
GType eam_action_get_type (void) G_GNUC_CONST;

GType
eam_action_get_type (void)
{
  static GType type = 0;

  if (type == 0)
  {
    static const GEnumValue values[] = {
      { EAM_ACTION_INSTALL, "EAM_ACTION_INSTALL", "install" },
      { EAM_ACTION_UPDATE, "EAM_ACTION_UPATE", "update" },
      { EAM_ACTION_XDELTA_UPDATE, "EAM_ACTION_XDELTA_UPDATE", "xdelta" },
      { 0, NULL, NULL }
    };
    type = g_enum_register_static (
      g_intern_static_string ("EamAction"),
      values);
  }
  return type;
}

typedef struct _EamInstallPrivate	EamInstallPrivate;

struct _EamInstallPrivate
{
  gchar *appid;
  gchar *from_version;
  EamAction action;
  EamBundle *bundle;
  EamBundle *xdelta_bundle;
};

static void transaction_iface_init (EamTransactionInterface *iface);
static void eam_install_run_async (EamTransaction *trans, GCancellable *cancellable,
   GAsyncReadyCallback callback, gpointer data);
static gboolean eam_install_finish (EamTransaction *trans, GAsyncResult *res,
   GError **error);

G_DEFINE_TYPE_WITH_CODE (EamInstall, eam_install, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (EAM_TYPE_TRANSACTION, transaction_iface_init)
  G_ADD_PRIVATE (EamInstall));

enum
{
  PROP_APPID = 1,
  PROP_ACTION,
  PROP_FROM_VERSION,
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
    g_set_error (error, EAM_TRANSACTION_ERROR, EAM_TRANSACTION_ERROR_INVALID_FILE,
       _("Not valid application link"));
    return NULL;
  }

  if (!sign) {
    g_set_error (error, EAM_TRANSACTION_ERROR, EAM_TRANSACTION_ERROR_INVALID_FILE,
       _("Not valid signature link"));
    return NULL;
  }

  EamBundle *bundle = g_slice_new (EamBundle);
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
eam_install_reset (EamInstall *self)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  g_clear_pointer (&priv->appid, g_free);
  g_clear_pointer (&priv->from_version, g_free);
  g_clear_pointer (&priv->bundle, eam_bundle_free);
  g_clear_pointer (&priv->xdelta_bundle, eam_bundle_free);
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
  case PROP_ACTION:
    priv->action = g_value_get_enum (value);
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
eam_install_get_property (GObject *obj, guint prop_id, GValue *value,
  GParamSpec *pspec)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (EAM_INSTALL (obj));

  switch (prop_id) {
  case PROP_APPID:
    g_value_set_string (value, priv->appid);
    break;
  case PROP_ACTION:
    g_value_set_enum (value, priv->action);
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
   * EamInstall:action:
   *
   * The action type to perform: install or update.
   */
  g_object_class_install_property (object_class, PROP_ACTION,
    g_param_spec_enum ("action", "Action type", "Action type", EAM_TYPE_ACTION,
      EAM_ACTION_INSTALL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_PRIVATE));

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
eam_install_init (EamInstall *self)
{
}

/**
 * eam_install_new:
 * @appid: the application ID to install.
 *
 * Returns: a new instance of #EamInstall with #EamTransaction interface.
 */
EamTransaction *
eam_install_new (const gchar *appid)
{
  return g_object_new (EAM_TYPE_INSTALL, "appid", appid, NULL);
}

/**
 * eam_install_new_from_version:
 * @appid: the application ID to update.
 * @from_version: the application's version that is currently installed
 *
 * Returns: a new instance of #EamTransaction able to update the @appid
 * application.
 */
EamTransaction *
eam_install_new_from_version (const gchar *appid, const gchar *from_version)
{
  return g_object_new (EAM_TYPE_INSTALL, "appid", appid,
    "from-version", from_version, "action", EAM_ACTION_XDELTA_UPDATE, NULL);
}

static gchar *
build_tarball_filename (const gchar *appid)
{
  gchar *fname = g_strconcat (appid, ".bundle", NULL);
  gchar *ret = g_build_filename (eam_config_dldir (), fname, NULL);
  g_free (fname);
  return ret;
}

static const gchar *
get_rollback_scriptdir (EamInstall *self)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  switch (priv->action) {
  case EAM_ACTION_INSTALL:
    return INSTALL_ROLLBACKDIR;
  case EAM_ACTION_UPDATE:
    return UPDATE_ROLLBACKDIR;
  case EAM_ACTION_XDELTA_UPDATE:
    return XDELTA_UPDATE_ROLLBACKDIR;
  default:
    g_assert_not_reached ();
  }
}

static const gchar *
get_scriptdir (EamInstall *self)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  switch (priv->action) {
  case EAM_ACTION_INSTALL:
    return INSTALL_SCRIPTDIR;
  case EAM_ACTION_UPDATE:
    return UPDATE_SCRIPTDIR;
  case EAM_ACTION_XDELTA_UPDATE:
    return XDELTA_UPDATE_SCRIPTDIR;
  default:
    g_assert_not_reached ();
  }
}

static void
run_scripts (EamInstall *self, const gchar *scriptdir,
  GCancellable *cancellable, GAsyncReadyCallback callback, GTask *task)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  /* scripts directory path */
  char *dir = g_build_filename (eam_config_scriptdir (), scriptdir, NULL);

  /* scripts parameters */
  GStrv params = g_new0 (gchar *, 3);
  params[0] = g_strdup (priv->appid);
  params[1] = build_tarball_filename (priv->appid);

  /* prefix environment */
  g_setenv ("EAM_PREFIX", eam_config_appdir (), FALSE);
  g_setenv ("EAM_TMP", eam_config_dldir (), FALSE);
  g_setenv ("EAM_GPGKEYRING", eam_config_gpgkeyring (), FALSE);

  EamSpawner *spawner = eam_spawner_new (dir, (const gchar * const *) params);
  eam_spawner_run_async (spawner, cancellable, callback, task);

  g_free (dir);
  g_strfreev (params);
  g_object_unref (spawner);
}

static void download_bundle (EamInstall *self, GTask *task);

static void
xdelta_rollback_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GTask *task = data;
  EamInstall *self = EAM_INSTALL (g_task_get_source_object (task));
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  /* Fallback mode: do a full update */
  priv->action = EAM_ACTION_UPDATE;
  download_bundle (self, task);
}

static void
rollback (GTask *task, GError *error)
{
  EamInstall *self = EAM_INSTALL (g_task_get_source_object (task));
  EamInstallPrivate *priv = eam_install_get_instance_private (self);
  GCancellable *cancellable = g_task_get_cancellable (task);

  if (priv->action == EAM_ACTION_XDELTA_UPDATE && priv->bundle) {
    run_scripts (self, get_rollback_scriptdir (self), cancellable,
      xdelta_rollback_cb, task);

    g_warning ("Incremental update failed: %s", error->message);
    g_clear_error (&error);
  } else {
    /* Use cancellable == NULL as we are returning immediately after
       using g_task_return. */
    run_scripts (self, get_rollback_scriptdir (self), NULL, NULL, NULL);

    g_task_return_error (task, error);
    g_object_unref (task);
  }
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

static inline gchar *
build_sha256sum_filename (const gchar *appid)
{
  gchar *fname = g_strconcat (appid, ".sha256", NULL);
  gchar *ret = g_build_filename (eam_config_dldir (), fname, NULL);
  g_free (fname);
  return ret;
}

static inline void
create_sha256sum_file (EamInstall *self, const gchar *tarball, GError **error)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  gchar *filename = build_sha256sum_filename (priv->appid);
  const gchar *hash;
  if (priv->action == EAM_ACTION_XDELTA_UPDATE)
    hash = priv->xdelta_bundle->hash;
  else
    hash = priv->bundle->hash;

  gchar *contents = g_strconcat (hash, "\t", tarball, "\n", NULL);

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

  EamInstall *self = EAM_INSTALL (g_task_get_source_object (task));
  EamInstallPrivate *priv = eam_install_get_instance_private (self);
  gchar *tarball = build_tarball_filename (priv->appid);
  create_sha256sum_file (self, tarball, &error);
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
  EamInstall *self = EAM_INSTALL (g_task_get_source_object (task));
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  gchar *filename = build_sign_filename (priv->appid);
  const gchar *download_url;
  if (priv->action == EAM_ACTION_XDELTA_UPDATE)
    download_url = priv->xdelta_bundle->signature_url;
  else
    download_url = priv->bundle->signature_url;

  GCancellable *cancellable = g_task_get_cancellable (task);
  EamWc *wc = eam_wc_new ();
  eam_wc_request_async (wc, download_url, filename, cancellable, dl_sign_cb, task);

  g_object_unref (wc);
  g_free (filename);

  return;

bail:
  g_object_unref (task);
}

static void
download_bundle (EamInstall *self, GTask *task)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  gchar *filename = build_tarball_filename (priv->appid);
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
xdelta_is_candidate (EamInstall *self, JsonObject *json)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

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
parse_json (EamInstall *self, JsonNode *root,
  JsonObject **bundle_json, JsonObject **xdelta_json)
{
  JsonObject *json = NULL;
  gboolean is_diff;
  const gchar *appid;

  *xdelta_json = NULL;
  *bundle_json = NULL;

  EamInstallPrivate *priv = eam_install_get_instance_private (self);

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
load_bundle_info (EamInstall *self, JsonNode *root)
{
  JsonObject *xdelta_json = NULL;
  JsonObject *bundle_json = NULL;

  parse_json (self, root, &bundle_json, &xdelta_json);

  EamInstallPrivate *priv = eam_install_get_instance_private (self);

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
  case EAM_ACTION_INSTALL:
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

  EamInstall *self = EAM_INSTALL (g_task_get_source_object (task));

  if (!load_bundle_info (self, json_parser_get_root (parser))) {
    g_task_return_new_error (task, EAM_TRANSACTION_ERROR,
       EAM_TRANSACTION_ERROR_INVALID_FILE,
       _("Not valid stream with update/install link"));
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
request_json_updates (EamInstall *self, GTask *task)
{
  /* is it enough? */
  if (!g_network_monitor_get_network_available (g_network_monitor_get_default ())) {
    g_task_return_new_error (task, EAM_TRANSACTION_ERROR,
      EAM_TRANSACTION_ERROR_NO_NETWORK, _("Networking is not available"));
    goto bail;
  }

  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  gchar *uri = eam_rest_build_uri (EAM_REST_API_V1_GET_APP_UPDATE_LINK,
    priv->appid, NULL, NULL);
  if (!uri) {
    g_task_return_new_error (task, EAM_TRANSACTION_ERROR,
      EAM_TRANSACTION_ERROR_PROTOCOL_ERROR, _("Not valid method or protocol version"));
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
build_json_updates_filename ()
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
    g_warning ("Can't load cached updates.json: %s", error->message);
    g_clear_error (&error);

    request_json_updates (self, task);
    goto bail;
  }

  if (!load_bundle_info (self, json_parser_get_root (parser))) {
    g_task_return_new_error (task, EAM_TRANSACTION_ERROR,
       EAM_TRANSACTION_ERROR_INVALID_FILE,
       _("Not valid stream with update/install link"));

    g_object_unref (task);
    goto bail;
  }

  download_bundle (self, task);

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
