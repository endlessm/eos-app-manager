/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include "eam-uninstall.h"

#include "eam-config.h"
#include "eam-error.h"
#include "eam-fs-sanity.h"
#include "eam-log.h"
#include "eam-utils.h"

typedef struct _EamUninstallPrivate	EamUninstallPrivate;

struct _EamUninstallPrivate
{
  char *appid;

  char *prefix;

  guint is_force : 1;
};

static void transaction_iface_init (EamTransactionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EamUninstall, eam_uninstall, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (EAM_TYPE_TRANSACTION, transaction_iface_init)
  G_ADD_PRIVATE (EamUninstall))

enum {
  PROP_APPID = 1
};

static gboolean
eam_uninstall_run_sync (EamTransaction *trans,
                        GCancellable *cancellable,
                        GError **error)
{
  if (!eam_fs_sanity_check ()) {
    g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                         "Unable to access applications directory");
    return FALSE;
  }

  EamUninstall *self = (EamUninstall *) trans;
  EamUninstallPrivate *priv = eam_uninstall_get_instance_private (self);

  if (priv->prefix == NULL)
    priv->prefix = g_strdup (eam_config_appdir ());

  if (!eam_utils_app_is_installed (priv->prefix, priv->appid)) {
    if (!priv->is_force) {
      g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                           "Application is not installed");
      return FALSE;
    }
    else {
      eam_log_error_message ("Application is not installed, but force is enabled");
    } 
  }

  /* Remove the symbolic links first, so even if any later operation
   * fails, the application won't appear in the system.
   *
   * XXX: On failure we may end up "leaking" the app, and wasting
   * space; we should have a way to do "garbage collection" at boot
   * or at shutdown, to remove uninstalled bundles that are still on
   * disk, and reclaim space.
   */
  eam_fs_prune_symlinks (priv->prefix, priv->appid);

  if (!eam_fs_prune_dir (priv->prefix, priv->appid)) {
    if (!priv->is_force) {
      g_set_error_literal (error, EAM_ERROR, EAM_ERROR_FAILED,
                           "Unable to remove the bundle files");
      return FALSE;
    }
    else {
      eam_log_error_message ("Unable to remove all the bundle files, but force is enabled");
    }
  }

  /* This is not fatal */
  if (!eam_utils_update_desktop (priv->prefix))
    eam_log_error_message ("Could not update the desktop's metadata");

  return TRUE;
}

static void
uninstall_thread_cb (GTask *task,
                     gpointer source_obj,
                     gpointer task_data,
                     GCancellable *cancellable)
{
  GError *error = NULL;

  if (!eam_uninstall_run_sync (source_obj, cancellable, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * eam_uninstall_run_async:
 * @trans: a #EamUninstall instance with #EamTransaction interface.
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore
 * @callback: (scope async): callback to call when the request is satisfied
 * @data: (closure): the data to pass to callback function
 *
 * Runs the 'uninstall' scripts.
 *
 */
static void
eam_uninstall_run_async (EamTransaction *trans,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer data)
{
  g_return_if_fail (EAM_IS_UNINSTALL (trans));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  EamUninstall *self = EAM_UNINSTALL (trans);

  GTask *task = g_task_new (self, cancellable, callback, data);

  g_task_run_in_thread (task, uninstall_thread_cb);
  g_object_unref (task);
}

/*
 * eam_uninstall_finish:
 * @trans:A #EamUninstall instance with #EamTransaction interface.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Finishes the uninstall transaction.
 *
 * Returns: %TRUE if everything went well, %FALSE otherwise
 **/
static gboolean
eam_uninstall_finish (EamTransaction *trans, GAsyncResult *res, GError **error)
{
  g_return_val_if_fail (EAM_IS_UNINSTALL (trans), FALSE);
  g_return_val_if_fail (g_task_is_valid (res, EAM_UNINSTALL (trans)), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}

static void
transaction_iface_init (EamTransactionInterface *iface)
{
  iface->run_sync = eam_uninstall_run_sync;
  iface->run_async = eam_uninstall_run_async;
  iface->finish = eam_uninstall_finish;
}

static void
eam_uninstall_finalize (GObject *obj)
{
  EamUninstallPrivate *priv = eam_uninstall_get_instance_private (EAM_UNINSTALL (obj));

  g_free (priv->appid);
  g_free (priv->prefix);

  G_OBJECT_CLASS (eam_uninstall_parent_class)->finalize (obj);
}

static void
eam_uninstall_get_property (GObject *obj, guint prop_id, GValue *value,
  GParamSpec *pspec)
{
  EamUninstallPrivate *priv = eam_uninstall_get_instance_private (EAM_UNINSTALL (obj));

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
eam_uninstall_set_property (GObject *obj, guint prop_id, const GValue *value,
  GParamSpec *pspec)
{
  EamUninstallPrivate *priv = eam_uninstall_get_instance_private (EAM_UNINSTALL (obj));

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
eam_uninstall_class_init (EamUninstallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eam_uninstall_finalize;
  object_class->get_property = eam_uninstall_get_property;
  object_class->set_property = eam_uninstall_set_property;

  /**
   * EamUninstall:appid:
   *
   * The application ID to uninstall.
   */
  g_object_class_install_property (object_class, PROP_APPID,
    g_param_spec_string ("appid", "App ID", "Application ID", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
eam_uninstall_init (EamUninstall *self)
{
  /* initialize prefix to the default */
  eam_uninstall_set_prefix (self, NULL);
}

/**
 * eam_uninstall_new:
 * @appid: the application ID to uninstall.
 *
 * Returns: a new instance of #EamUninstall with #EamTransaction interface.
 */
EamTransaction *
eam_uninstall_new (const gchar *appid)
{
  return g_object_new (EAM_TYPE_UNINSTALL,
		       "appid", appid,
		       NULL);
}

void
eam_uninstall_set_force (EamUninstall *uninstall,
                         gboolean      force)
{
  EamUninstallPrivate *priv = eam_uninstall_get_instance_private (uninstall);

  g_return_if_fail (EAM_IS_UNINSTALL (uninstall));

  priv->is_force = !!force;
}

void
eam_uninstall_set_prefix (EamUninstall *uninstall,
                          const char   *prefix)
{
  EamUninstallPrivate *priv = eam_uninstall_get_instance_private (uninstall);

  g_return_if_fail (EAM_IS_UNINSTALL (uninstall));

  if (g_strcmp0 (priv->prefix, prefix) == 0)
    return;

  g_free (priv->prefix);
  priv->prefix = g_strdup (priv->prefix);
}
