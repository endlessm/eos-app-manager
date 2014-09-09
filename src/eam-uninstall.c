/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include "eam-uninstall.h"
#include "eam-spawner.h"
#include "eam-config.h"

typedef struct _EamUninstallPrivate	EamUninstallPrivate;

struct _EamUninstallPrivate
{
  gchar *appid;
};

static void transaction_iface_init (EamTransactionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EamUninstall, eam_uninstall, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (EAM_TYPE_TRANSACTION, transaction_iface_init)
  G_ADD_PRIVATE (EamUninstall));

enum
{
  PROP_APPID = 1,
};


static void
run_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GTask *task = data;
  GError *error = NULL;

  gboolean ret = eam_spawner_run_finish (EAM_SPAWNER (source), result, &error);
  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  if (g_task_return_error_if_cancelled (task))
    goto bail;

  g_task_return_boolean (task, ret);

bail:
  g_object_unref (task);
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
eam_uninstall_run_async (EamTransaction *trans, GCancellable *cancellable,
                         GAsyncReadyCallback callback, gpointer data)
{
  g_return_if_fail (EAM_IS_UNINSTALL (trans));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  EamUninstall *self = EAM_UNINSTALL (trans);
  EamUninstallPrivate *priv = eam_uninstall_get_instance_private (self);
  GTask *task = g_task_new (self, cancellable, callback, data);

  char *dir = g_build_filename (eam_config_scriptdir (), "uninstall", NULL);
  g_setenv ("EAM_PREFIX", eam_config_appdir (), FALSE);
  g_setenv ("EAM_TMP", eam_config_dldir (), FALSE);

  GStrv params = g_new0 (gchar *, 2);
  params[0] = g_strdup (priv->appid);

  EamSpawner *spawner = eam_spawner_new (dir, (const gchar * const *) params);
  eam_spawner_run_async (spawner, cancellable, run_cb, task);

  g_free (dir);
  g_strfreev (params);
  g_object_unref (spawner);
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
  iface->run_async = eam_uninstall_run_async;
  iface->finish = eam_uninstall_finish;
}

static void
eam_uninstall_finalize (GObject *obj)
{
  EamUninstallPrivate *priv = eam_uninstall_get_instance_private (EAM_UNINSTALL (obj));

  g_clear_pointer (&priv->appid, g_free);

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
  return g_object_new (EAM_TYPE_UNINSTALL, "appid", appid, NULL);
}
