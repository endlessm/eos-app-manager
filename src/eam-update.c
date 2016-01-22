/* eam-update.c: Update transaction
 *
 * This file is part of eos-app-manager.
 * Copyright 2014  Endless Mobile Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib/gi18n.h>

#include "eam-update.h"

#include "eam-config.h"
#include "eam-error.h"
#include "eam-fs-utils.h"
#include "eam-log.h"
#include "eam-utils.h"

#define XDELTA_BUNDLE_EXT               "xdelta"
#define INSTALL_BUNDLE_EXT              "bundle"
#define INSTALL_BUNDLE_SIGNATURE_EXT    "asc"

typedef struct _EamUpdatePrivate        EamUpdatePrivate;

struct _EamUpdatePrivate
{
  gchar *appid;

  /* Where the app is */
  char *source_prefix;
  /* Where the app should be */
  char *target_prefix;

  char *bundle_file;
  char *signature_file;
};

static void transaction_iface_init (EamTransactionInterface *iface);
static gboolean eam_update_run_sync (EamTransaction *trans,
                                     GCancellable *cancellable,
                                     GError **error);
static void eam_update_run_async (EamTransaction *trans,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer data);
static gboolean eam_update_finish (EamTransaction *trans,
                                   GAsyncResult *res,
                                   GError **error);

G_DEFINE_TYPE_WITH_CODE (EamUpdate, eam_update, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (EAM_TYPE_TRANSACTION, transaction_iface_init)
  G_ADD_PRIVATE (EamUpdate));

enum
{
  PROP_APPID = 1,
};

static void
transaction_iface_init (EamTransactionInterface *iface)
{
  iface->run_sync = eam_update_run_sync;
  iface->run_async = eam_update_run_async;
  iface->finish = eam_update_finish;
}

static void
eam_update_finalize (GObject *obj)
{
  EamUpdate *self = EAM_UPDATE (obj);
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  g_free (priv->appid);
  g_free (priv->bundle_file);
  g_free (priv->signature_file);
  g_free (priv->source_prefix);
  g_free (priv->target_prefix);

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
}

static void
eam_update_init (EamUpdate *self)
{
  eam_update_set_source_prefix (self, NULL);
  eam_update_set_target_prefix (self, NULL);
}

/**
 * eam_update_new:
 * @appid: the application ID to update.
 *
 * Returns: a new instance of #EamUpdate with #EamTransaction interface.
 */
EamTransaction *
eam_update_new (const gchar *appid)
{
  return g_object_new (EAM_TYPE_UPDATE,
                       "appid", appid,
                       NULL);
}

static gboolean
do_xdelta_update (const char *prefix,
                  const char *appid,
                  const char *source_dir,
                  const char *delta_file,
                  GCancellable *cancellable,
                  GError **error)
{
  eam_utils_cleanup_python (source_dir);

  if (!eam_utils_apply_xdelta (source_dir, appid, delta_file, cancellable)) {
    if (g_cancellable_is_cancelled (cancellable))
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Operation cancelled");
    else
      g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                           "Could not update the application via xdelta");
    return FALSE;
  }

  /* Deploy the appdir from the extraction directory to the app directory */
  if (!eam_fs_deploy_app (eam_config_get_cache_dir (), prefix, appid, cancellable)) {
    eam_fs_prune_dir (eam_config_get_cache_dir (), appid);
    if (g_cancellable_is_cancelled (cancellable))
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Operation cancelled");
    else
      g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                           "Could not deploy the bundle in the application directory");

    return FALSE;
  }

  return TRUE;
}

static gboolean
do_full_update (const char *prefix,
                const char *appid,
                const char *bundle_file,
                GCancellable *cancellable,
                GError **error)
{
  if (!eam_utils_bundle_extract (bundle_file, eam_config_get_cache_dir (), appid, cancellable)) {
    eam_fs_prune_dir (eam_config_get_cache_dir (), appid);
    if (g_cancellable_is_cancelled (cancellable))
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Operation cancelled");
    else
      g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                           "Could not extract the bundle");
    return FALSE;
  }

  /* run 3rd party scripts */
  if (!eam_utils_run_external_scripts (eam_config_get_cache_dir (), appid, cancellable)) {
    eam_fs_prune_dir (eam_config_get_cache_dir (), appid);
    if (g_cancellable_is_cancelled (cancellable))
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Operation cancelled");
    else
      g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                           "Could not process the external script");
    return FALSE;
  }

  /* Deploy the appdir from the extraction directory to the app directory */
  if (!eam_fs_deploy_app (eam_config_get_cache_dir (), prefix, appid, cancellable)) {
    eam_fs_prune_dir (eam_config_get_cache_dir (), appid);
    if (g_cancellable_is_cancelled (cancellable))
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Operation cancelled");
    else
      g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                           "Could not deploy the bundle in the application directory");
    return FALSE;
  }

  return TRUE;
}

static gboolean
eam_update_run_sync (EamTransaction *trans,
                     GCancellable *cancellable,
                     GError **error)
{
  EamUpdate *self = (EamUpdate *) trans;
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  if (priv->bundle_file == NULL) {
    g_set_error_literal (error, EAM_ERROR, EAM_ERROR_INVALID_FILE,
                         "No bundle location set");
    return FALSE;
  }

  if (!g_file_test (priv->bundle_file, G_FILE_TEST_EXISTS)) {
    g_set_error_literal (error, EAM_ERROR, EAM_ERROR_INVALID_FILE,
                         "No bundle file found");
    return FALSE;
  }

  /* If we don't have an explicit signature file, we're going to look for one
   * in the same directory as the bundle, using the appid as the basename
   */
  if (priv->signature_file == NULL) {
    g_autofree char *dirname = g_path_get_dirname (priv->bundle_file);
    g_autofree char *filename = g_strconcat (priv->appid, ".", INSTALL_BUNDLE_SIGNATURE_EXT, NULL);

    priv->signature_file = g_build_filename (dirname, filename, NULL);
  }

  if (!eam_fs_sanity_check ()) {
    g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                         "Unable to access applications directory");
    return FALSE;
  }

  if (!eam_utils_app_is_installed (priv->source_prefix, priv->appid)) {
    g_set_error (error, EAM_ERROR, EAM_ERROR_FAILED,
                 "Application '%s' is not installed",
                 priv->appid);
    return FALSE;
  }

  if (!eam_utils_verify_signature (priv->bundle_file, priv->signature_file, cancellable)) {
    if (g_cancellable_is_cancelled (cancellable))
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Operation cancelled");
    else
      g_set_error_literal (error, EAM_ERROR, EAM_ERROR_INVALID_FILE,
                           "The signature for the application bundle is invalid");
    return FALSE;
  }

  /* Keep a copy of the old app around, in case the update fails */
  g_autofree char *backupdir = NULL;
  if (!eam_fs_backup_app (priv->source_prefix, priv->appid, &backupdir)) {
    g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                         "Could not keep a copy of the app");
    return FALSE;
  }

  GError *internal_error = NULL;
  gboolean res;

  if (g_str_has_suffix (priv->bundle_file, INSTALL_BUNDLE_EXT))
    res = do_full_update (priv->target_prefix, priv->appid, priv->bundle_file, cancellable, &internal_error);
  else if (g_str_has_suffix (priv->bundle_file, XDELTA_BUNDLE_EXT))
    res = do_xdelta_update (priv->target_prefix, priv->appid, backupdir, priv->bundle_file, cancellable, &internal_error);
  else
    g_assert_not_reached ();

  /* If the update failed, restore from backup and bail out */
  if (!res) {
    if (backupdir)
      eam_fs_restore_app (priv->source_prefix, priv->appid, backupdir);

    g_propagate_error (error, internal_error);
    return FALSE;
  }

  /* If the symbolic link creation fails, we restore from backup */
  res = eam_fs_create_symlinks (priv->target_prefix, priv->appid);
  if (!res) {
    if (backupdir)
      eam_fs_restore_app (priv->source_prefix, priv->appid, backupdir);

    g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                         "Could not create symbolic links");
    return FALSE;
  }

  /* These two errors are non-fatal */
  if (!eam_utils_compile_python (priv->target_prefix, priv->appid)) {
    eam_log_error_message ("Python libraries compilation failed");
  }

  if (!eam_utils_update_desktop ()) {
    eam_log_error_message ("Could not update the desktop's metadata");
  }

  /* The update was successful; we can delete the back up directory */
  if (backupdir)
    eam_fs_rmdir_recursive (backupdir);

  return TRUE;
}

static void
update_thread_cb (GTask *task,
                  gpointer source_obj,
                  gpointer task_data,
                  GCancellable *cancellable)
{
  GError *error = NULL;
  if (!eam_update_run_sync (source_obj, cancellable, &error)) {
    g_task_return_error (task, error);
    return;
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

  GTask *task = g_task_new (self, cancellable, callback, data);
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
eam_update_set_bundle_file (EamUpdate *update,
                            const char *path)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (update);

  g_free (priv->bundle_file);
  priv->bundle_file = g_strdup (path);
}

void
eam_update_set_signature_file (EamUpdate *update,
                               const char *path)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (update);

  g_free (priv->signature_file);
  priv->signature_file = g_strdup (path);
}

void
eam_update_set_source_prefix (EamUpdate *update,
                              const char *path)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (update);
  const char *prefix;

  if (path == NULL || *path == '\0')
    prefix = eam_config_get_applications_dir ();
  else
    prefix = path;

  if (g_strcmp0 (priv->source_prefix, prefix) == 0)
    return;

  g_free (priv->source_prefix);
  priv->source_prefix = g_strdup (prefix);
}

void
eam_update_set_target_prefix (EamUpdate *update,
                              const char *path)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (update);
  const char *prefix;

  if (path == NULL || *path == '\0')
    prefix = eam_config_get_applications_dir ();
  else
    prefix = path;

  if (g_strcmp0 (priv->target_prefix, prefix) == 0)
    return;

  g_free (priv->target_prefix);
  priv->target_prefix = g_strdup (prefix);
}

const char *
eam_update_get_app_id (EamUpdate *update)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (update);

  return priv->appid;
}
