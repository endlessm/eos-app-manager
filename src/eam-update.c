/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include <glib/gi18n.h>

#include "eam-log.h"
#include "eam-error.h"
#include "eam-update.h"
#include "eam-spawner.h"
#include "eam-config.h"
#include "eam-utils.h"
#include "eam-fs-sanity.h"

#define UPDATE_SCRIPTDIR "update/full"
#define UPDATE_ROLLBACKDIR "update/rollback"
#define XDELTA_UPDATE_SCRIPTDIR "update/xdelta"
#define XDELTA_UPDATE_ROLLBACKDIR "update/rollback"

#define XDELTA_BUNDLE_EXT               "xdelta"
#define INSTALL_BUNDLE_EXT              "bundle"
#define INSTALL_BUNDLE_DIGEST_EXT       "sha256"
#define INSTALL_BUNDLE_SIGNATURE_EXT    "asc"

typedef enum {
  EAM_ACTION_FULL_UPDATE,   /* Update downloading the complete bundle */
  EAM_ACTION_XDELTA_UPDATE, /* Update applying xdelta diff files */
} EamAction;

typedef struct _EamUpdatePrivate        EamUpdatePrivate;

struct _EamUpdatePrivate
{
  gchar *appid;
  gboolean allow_deltas;
  char *bundle_location;
  EamAction action;
};

static void transaction_iface_init (EamTransactionInterface *iface);
static void eam_update_run_async (EamTransaction *trans, GCancellable *cancellable,
  GAsyncReadyCallback callback, gpointer data);
static gboolean eam_update_finish (EamTransaction *trans, GAsyncResult *res,
  GError **error);

G_DEFINE_TYPE_WITH_CODE (EamUpdate, eam_update, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (EAM_TYPE_TRANSACTION, transaction_iface_init)
  G_ADD_PRIVATE (EamUpdate));

enum
{
  PROP_APPID = 1,
  PROP_ALLOW_DELTAS
};

static void
transaction_iface_init (EamTransactionInterface *iface)
{
  iface->run_async = eam_update_run_async;
  iface->finish = eam_update_finish;
}

static void
eam_update_finalize (GObject *obj)
{
  EamUpdate *self = EAM_UPDATE (obj);
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  g_clear_pointer (&priv->appid, g_free);
  g_clear_pointer (&priv->bundle_location, g_free);

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
  case PROP_ALLOW_DELTAS:
    priv->allow_deltas = g_value_get_boolean (value);
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
  case PROP_ALLOW_DELTAS:
    g_value_set_boolean (value, priv->allow_deltas);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
eam_update_constructed (GObject *gobject)
{
  EamUpdate *self = EAM_UPDATE (gobject);
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  if (eam_config_deltaupdates () && priv->allow_deltas)
    priv->action = EAM_ACTION_XDELTA_UPDATE;
  else
    priv->action = EAM_ACTION_FULL_UPDATE;

  G_OBJECT_CLASS (eam_update_parent_class)->constructed (gobject);
}

static void
eam_update_class_init (EamUpdateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eam_update_finalize;
  object_class->get_property = eam_update_get_property;
  object_class->set_property = eam_update_set_property;
  object_class->constructed = eam_update_constructed;

  /**
   * EamUpdate:appid:
   *
   * The application ID to update
   */
  g_object_class_install_property (object_class, PROP_APPID,
    g_param_spec_string ("appid", "App ID", "Application ID", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * EamUpdate:allow_deltas:
   *
   * Whether we want to allow delta updates or not
   */
  g_object_class_install_property (object_class, PROP_ALLOW_DELTAS,
    g_param_spec_boolean ("allow-deltas", "Allow Deltas", "Allow Deltas", TRUE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
eam_update_init (EamUpdate *self)
{
}

/**
 * eam_update_new:
 * @appid: the application ID to update.
 * @allow_deltas: whether to allow incremental updates
 *
 * Returns: a new instance of #EamUpdate with #EamTransaction interface.
 */
EamTransaction *
eam_update_new (const gchar *appid,
                gboolean allow_deltas)
{
  return g_object_new (EAM_TYPE_UPDATE,
                       "appid", appid,
                       "allow-deltas", allow_deltas,
                       NULL);
}

static char *
build_bundle_filename (const char *basedir,
                       const char *basename,
                       const char *extension)
{
  g_autofree char *filename = g_strconcat (basename, ".", extension, NULL);

  return g_build_filename (basedir, filename, NULL);
}

static char *
build_tarball_filename (EamUpdate *self)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  if (priv->action == EAM_ACTION_XDELTA_UPDATE)
    return build_bundle_filename (priv->bundle_location, priv->appid, XDELTA_BUNDLE_EXT);

  return build_bundle_filename (priv->bundle_location, priv->appid, INSTALL_BUNDLE_EXT);
}

static char *
build_checksum_filename (EamUpdate *self)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  return build_bundle_filename (priv->bundle_location, priv->appid, INSTALL_BUNDLE_DIGEST_EXT);
}

static char *
build_signature_filename (EamUpdate *self)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  return build_bundle_filename (priv->bundle_location, priv->appid, INSTALL_BUNDLE_SIGNATURE_EXT);
}

static gboolean
do_xdelta_update (const char *appid,
                  const char *delta_file,
                  GError **error)
{
  eam_utils_cleanup_python (eam_config_appdir (), appid);

  if (!eam_utils_apply_xdelta (eam_config_appdir (), appid, delta_file)) {
    g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                         "Could not update the application via xdelta");
    return FALSE;
  }

  return TRUE;
}

static gboolean
do_full_update (const char *appid,
                const char *bundle_file,
                GError **error)
{
  if (!eam_utils_bundle_extract (bundle_file, eam_config_dldir (), appid)) {
    eam_fs_prune_dir (eam_config_dldir (), appid);
    g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                         "Could not extract the bundle");
    return FALSE;
  }

  /* run 3rd party scripts */
  if (!eam_utils_run_external_scripts (eam_config_dldir (), appid)) {
    eam_fs_prune_dir (eam_config_dldir (), appid);
    g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                         "Could not process the external script");
    return FALSE;
  }

  /* Deploy the appdir from the extraction directory to the app directory */
  if (!eam_fs_deploy_app (eam_config_dldir (), eam_config_appdir (), appid)) {
    eam_fs_prune_dir (eam_config_dldir (), appid);

    g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                         "Could not deploy the bundle in the application directory");
    return FALSE;
  }

  return TRUE;
}

static void
update_thread_cb (GTask *task,
                  gpointer source_obj,
                  gpointer task_data,
                  GCancellable *cancellable)
{
  if (!eam_fs_sanity_check ()) {
    g_task_return_new_error (task, EAM_ERROR, EAM_ERROR_FAILED,
                             "Unable to access applications directory");
    return;
  }

  EamUpdate *self = source_obj;
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  if (!eam_utils_app_is_installed (eam_config_appdir(), priv->appid)) {
    g_task_return_new_error (task, EAM_ERROR, EAM_ERROR_FAILED,
                             "Application is not installed");
    return;
  }

  g_autofree char *bundle_file = build_tarball_filename (self);
  g_autofree char *checksum_file = build_checksum_filename (self);
  if (!eam_utils_verify_checksum (bundle_file, checksum_file)) {
    g_task_return_new_error (task, EAM_ERROR, EAM_ERROR_INVALID_FILE,
                             "The checksum for the application bundle is invalid");
    return;
  }

  g_autofree char *signature_file = build_signature_filename (self);
  if (!eam_utils_verify_signature (bundle_file, signature_file)) {
    g_task_return_new_error (task, EAM_ERROR, EAM_ERROR_INVALID_FILE,
                             "The signature for the application bundle is invalid");
    return;
  }

  /* Keep a copy of the old app around, in case the update fails */
  g_autofree char *backupdir = NULL;
  if (!eam_fs_backup_app (eam_config_appdir (), priv->appid, &backupdir)) {
    eam_fs_prune_dir (eam_config_dldir (), priv->appid);
    g_task_return_new_error (task, EAM_ERROR, EAM_ERROR_FAILED,
                             "Could not keep a copy of the app");
    return;
  }

  GError *error = NULL;
  gboolean res;

  switch (priv->action) {
    case EAM_ACTION_FULL_UPDATE:
      res = do_full_update (priv->appid, bundle_file, &error);
      break;

    case EAM_ACTION_XDELTA_UPDATE:
      res = do_xdelta_update (priv->appid, bundle_file, &error);
      break;

    default:
      g_assert_not_reached ();
  }

  if (!res) {
    eam_fs_restore_app (eam_config_appdir (), priv->appid, backupdir);
    g_task_return_error (task, error);
    return;
  }

  /* Remove the old app */
  if (backupdir != NULL) {
    eam_fs_rmdir_recursive (backupdir);
  }

  /* Build the symlink farm for files to appear in the OS locations */
  if (!eam_fs_create_symlinks (eam_config_appdir (), priv->appid)) {
    eam_fs_prune_symlinks (eam_config_appdir (), priv->appid);
    eam_fs_prune_dir (eam_config_appdir (), priv->appid);
    g_task_return_new_error (task, EAM_ERROR, EAM_ERROR_FAILED,
                             "Could not create all the symbolic links");
    return;
  }

  /* These two errors are non-fatal */
  if (!eam_utils_compile_python (eam_config_appdir (), priv->appid)) {
    eam_log_error_message ("Python libraries compilation failed");
  }

  if (!eam_utils_update_desktop (eam_config_appdir ())) {
    eam_log_error_message ("Could not update the desktop's metadata");
  }

  g_task_return_boolean (task, TRUE);
}

static void
eam_update_run_async (EamTransaction *trans,
                      GCancellable *cancellable,
                      GAsyncReadyCallback callback,
                      gpointer data)
{
  g_return_if_fail (EAM_IS_UPDATE (trans));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  EamUpdate *self = EAM_UPDATE (trans);
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  GTask *task = g_task_new (self, cancellable, callback, data);

  if (priv->bundle_location == NULL) {
    g_task_return_new_error (task, EAM_ERROR, EAM_ERROR_INVALID_FILE,
                             "No bundle location set");
    g_object_unref (task);
    return;
  }

  g_task_run_in_thread (task, update_thread_cb);
  g_object_unref (task);
}

static gboolean
eam_update_finish (EamTransaction *trans, GAsyncResult *res, GError **error)
{
  g_return_val_if_fail (EAM_IS_UPDATE (trans), FALSE);
  g_return_val_if_fail (g_task_is_valid (res, trans), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
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
