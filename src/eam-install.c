/* eam-install.c: Installation transaction
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

#include "eam-install.h"

#include "eam-config.h"
#include "eam-error.h"
#include "eam-fs-utils.h"
#include "eam-log.h"
#include "eam-utils.h"

#define INSTALL_BUNDLE_EXT              "bundle"

typedef struct _EamInstallPrivate        EamInstallPrivate;

struct _EamInstallPrivate
{
  char *appid;

  char *prefix;
  char *bundle_file;
  char *signature_file;

  gboolean skip_signature;
};

static void transaction_iface_init (EamTransactionInterface *iface);
static gboolean eam_install_run_sync (EamTransaction *trans,
                                      GCancellable *cancellable,
                                      GError **error);
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
  iface->run_sync = eam_install_run_sync;
  iface->run_async = eam_install_run_async;
  iface->finish = eam_install_finish;
}

static void
eam_install_finalize (GObject *obj)
{
  EamInstall *self = EAM_INSTALL (obj);
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  g_free (priv->appid);
  g_free (priv->bundle_file);
  g_free (priv->signature_file);
  g_free (priv->prefix);

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
  eam_install_set_prefix (self, NULL);
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

static gboolean
eam_install_run_sync (EamTransaction *trans,
                      GCancellable *cancellable,
                      GError **error)
{
  EamInstall *self = (EamInstall *) trans;

  EamInstallPrivate *priv = eam_install_get_instance_private (self);

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

  if (!eam_fs_sanity_check ()) {
    g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                         "Unable to access applications directory");
    return FALSE;
  }

  if (eam_utils_app_is_installed (priv->prefix, priv->appid)) {
    g_set_error (error, EAM_ERROR, EAM_ERROR_FAILED,
                 "Application '%s' already installed",
                 priv->appid);
    return FALSE;
  }

  if (!priv->skip_signature) {
    if (!eam_utils_verify_signature (priv->bundle_file, priv->signature_file, cancellable)) {
      if (g_cancellable_is_cancelled (cancellable))
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Operation cancelled");
      else
        g_set_error_literal (error, EAM_ERROR, EAM_ERROR_INVALID_FILE,
                             "The signature for the application bundle is invalid");
      return FALSE;
    }
  }

  /* Further operations require rollback */

  if (!eam_utils_bundle_extract (priv->bundle_file, eam_config_get_cache_dir (), priv->appid, cancellable)) {
    eam_fs_prune_dir (eam_config_get_cache_dir (), priv->appid);
    if (g_cancellable_is_cancelled (cancellable))
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Operation cancelled");
    else
      g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                           "Could not extract the bundle");
    return FALSE;
  }

  /* run 3rd party scripts */
  if (!eam_utils_run_external_scripts (eam_config_get_cache_dir (), priv->appid, cancellable)) {
    eam_fs_prune_dir (eam_config_get_cache_dir (), priv->appid);
    if (g_cancellable_is_cancelled (cancellable))
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Operation cancelled");
    else
      g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                           "Could not process the external script");
    return FALSE;
  }

  /* Deploy the appdir from the extraction directory to the app directory */
  if (!eam_fs_deploy_app (eam_config_get_cache_dir (), priv->prefix, priv->appid, cancellable)) {
    eam_fs_prune_dir (eam_config_get_cache_dir (), priv->appid);
    if (g_cancellable_is_cancelled (cancellable))
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Operation cancelled");
    else
      g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                           "Could not deploy the bundle in the application directory");
    return FALSE;
  }

  /* Build the symlink farm for files to appear in the OS locations */
  if (!eam_fs_create_symlinks (priv->prefix, priv->appid)) {
    eam_fs_prune_symlinks (priv->prefix, priv->appid);
    eam_fs_prune_dir (priv->prefix, priv->appid);
    g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                         "Could not create all the symbolic links");
    return FALSE;
  }

  /* These two errors are non-fatal */
  if (!eam_utils_compile_python (priv->prefix, priv->appid)) {
    eam_log_error_message ("Python libraries compilation failed");
  }

  if (!eam_utils_update_desktop ()) {
    eam_log_error_message ("Could not update the desktop's metadata");
  }

  return TRUE;
}

static void
install_thread_cb (GTask *task,
                   gpointer source_obj,
                   gpointer task_data,
                   GCancellable *cancellable)
{
  GError *error = NULL;
  eam_install_run_sync (source_obj, cancellable, &error);
  if (error != NULL) {
    g_task_return_error (task, error);
    return;
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
  GTask *task = g_task_new (self, cancellable, callback, data);
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
eam_install_set_bundle_file (EamInstall *install,
                             const char *path)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (install);

  g_free (priv->bundle_file);
  priv->bundle_file = g_strdup (path);
}

void
eam_install_set_signature_file (EamInstall *install,
                                const char *path)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (install);

  g_free (priv->signature_file);
  priv->signature_file = g_strdup (path);
}

void
eam_install_set_skip_signature (EamInstall *install,
                                gboolean skip_signature)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (install);
  priv->skip_signature = skip_signature;
}

void
eam_install_set_prefix (EamInstall *install,
                        const char *path)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (install);
  const char *prefix;

  if (path == NULL || *path == '\0')
    prefix = eam_config_get_primary_storage ();
  else
    prefix = path;

  if (g_strcmp0 (priv->prefix, prefix) == 0)
    return;

  g_free (priv->prefix);
  priv->prefix = g_strdup (prefix);
}

const char *
eam_install_get_app_id (EamInstall *install)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (install);

  return priv->appid;
}
