/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include <glib/gi18n.h>

#include "eam-log.h"
#include "eam-error.h"
#include "eam-install.h"
#include "eam-spawner.h"
#include "eam-config.h"
#include "eam-utils.h"
#include "eam-fs-sanity.h"

#define INSTALL_BUNDLE_EXT              "bundle"
#define INSTALL_BUNDLE_DIGEST_EXT       "sha256"
#define INSTALL_BUNDLE_SIGNATURE_EXT    "asc"

typedef struct _EamInstallPrivate        EamInstallPrivate;

struct _EamInstallPrivate
{
  gchar *appid;
  gchar *from_version;
  char *bundle_location;
};

static void transaction_iface_init (EamTransactionInterface *iface);
static void eam_install_run_async  (EamTransaction *trans,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer data);
static gboolean eam_install_finish (EamTransaction *trans,
                                    GAsyncResult *res,
                                    GError **error);

G_DEFINE_TYPE_WITH_CODE (EamInstall, eam_install, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (EAM_TYPE_TRANSACTION, transaction_iface_init)
  G_ADD_PRIVATE (EamInstall))

enum
{
  PROP_APPID = 1
};

static void
transaction_iface_init (EamTransactionInterface *iface)
{
  iface->run_async = eam_install_run_async;
  iface->finish = eam_install_finish;
}

static void
eam_install_finalize (GObject *obj)
{
  EamInstall *self = EAM_INSTALL (obj);
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  g_clear_pointer (&priv->appid, g_free);
  g_clear_pointer (&priv->bundle_location, g_free);

  G_OBJECT_CLASS (eam_install_parent_class)->finalize (obj);
}

static void
eam_install_set_property (GObject *obj,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (EAM_INSTALL (obj));

  switch (prop_id) {
  case PROP_APPID:
    priv->appid = g_value_dup_string (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
eam_install_get_property (GObject *obj,
                          guint prop_id,
                          GValue *value,
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

static char *
build_bundle_filename (const char *basedir,
                       const char *basename,
                       const char *extension)
{
  g_autofree char *filename = g_strconcat (basename, ".", extension, NULL);

  return g_build_filename (basedir, filename, NULL);
}

static char *
build_tarball_filename (EamInstall *self)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  return build_bundle_filename (priv->bundle_location, priv->appid, INSTALL_BUNDLE_EXT);
}

static char *
build_checksum_filename (EamInstall *self)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  return build_bundle_filename (priv->bundle_location, priv->appid, INSTALL_BUNDLE_DIGEST_EXT);
}

static char *
build_signature_filename (EamInstall *self)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  return build_bundle_filename (priv->bundle_location, priv->appid, INSTALL_BUNDLE_SIGNATURE_EXT);
}

static void
install_thread_cb (GTask *task,
                   gpointer source_obj,
                   gpointer task_data,
                   GCancellable *cancellable)
{
  if (!eam_fs_sanity_check ()) {
    g_task_return_new_error (task, EAM_ERROR, EAM_ERROR_FAILED,
                             "Unable to access applications directory");
    return;
  }

  EamInstall *self = source_obj;
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  if (eam_utils_app_is_installed (eam_config_appdir(), priv->appid)) {
    g_task_return_new_error (task, EAM_ERROR, EAM_ERROR_FAILED,
                             "Application already installed");
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

  /* Further operations require rollback */

  if (!eam_utils_bundle_extract (bundle_file, eam_config_dldir (), priv->appid)) {
    eam_fs_prune_dir (eam_config_dldir (), priv->appid);
    g_task_return_new_error (task, EAM_ERROR, EAM_ERROR_FAILED,
                             "Could not extract the bundle");
    return;
  }

  /* run 3rd party scripts */
  if (!eam_utils_run_external_scripts (eam_config_dldir (), priv->appid)) {
    eam_fs_prune_dir (eam_config_dldir (), priv->appid);
    g_task_return_new_error (task, EAM_ERROR, EAM_ERROR_FAILED,
                             "Could not process the external script");
    return;
  }

  /* Deploy the appdir from the extraction directory to the app directory */
  if (!eam_fs_deploy_app (eam_config_dldir (), eam_config_appdir (), priv->appid)) {
    eam_fs_prune_dir (eam_config_dldir (), priv->appid);
    g_task_return_new_error (task, EAM_ERROR, EAM_ERROR_FAILED,
                             "Could not deploy the bundle in the application directory");
    return;
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
eam_install_run_async (EamTransaction *trans,
                       GCancellable *cancellable,
                       GAsyncReadyCallback callback,
                       gpointer data)
{
  g_return_if_fail (EAM_IS_INSTALL (trans));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  EamInstall *self = EAM_INSTALL (trans);
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  GTask *task = g_task_new (self, cancellable, callback, data);

  if (priv->bundle_location == NULL) {
    g_task_return_new_error (task, EAM_ERROR, EAM_ERROR_INVALID_FILE,
                             "No bundle location set");
    g_object_unref (task);
    return;
  }

  g_task_run_in_thread (task, install_thread_cb);
  g_object_unref (task);
}

static gboolean
eam_install_finish (EamTransaction *trans, GAsyncResult *res, GError **error)
{
  g_return_val_if_fail (EAM_IS_INSTALL (trans), FALSE);
  g_return_val_if_fail (g_task_is_valid (res, trans), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
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
