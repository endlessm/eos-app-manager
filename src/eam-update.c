/* Copyright 2014 Endless Mobile, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "eam-update.h"
#include "eam-install.h"

#define SCRIPTDIR "update/full"
#define ROLLBACKDIR "update/rollback"

typedef struct _EamUpdatePrivate	EamUpdatePrivate;

struct _EamUpdatePrivate
{
  gchar *appid;
};

static void transaction_iface_init (EamTransactionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EamUpdate, eam_update, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (EAM_TYPE_TRANSACTION, transaction_iface_init)
  G_ADD_PRIVATE (EamUpdate));

enum
{
  PROP_APPID = 1,
};

static void
run_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  g_return_if_fail (EAM_IS_INSTALL (source));

  GTask *task = data;
  GError *error = NULL;

  gboolean ret = eam_install_finish (EAM_INSTALL (source), result, &error);
  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  if (g_task_return_error_if_cancelled (task))
    goto bail;

  g_task_return_boolean (task, ret);

bail:
  g_object_unref (source);
  g_object_unref (task);
}

/**
 * eam_update_run_async:
 * @trans: a #EamUpdate instance with #EamTransaction interface.
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore
 * @callback: (scope async): callback to call when the request is satisfied
 * @data: (closure): the data to pass to callback function
 *
 * Updates an application.
 */
static void
eam_update_run_async (EamTransaction *trans, GCancellable *cancellable,
  GAsyncReadyCallback callback, gpointer data)
{
  g_return_if_fail (EAM_IS_UPDATE (trans));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  EamUpdate *self = EAM_UPDATE (trans);
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);
  GTask *task = g_task_new (self, cancellable, callback, data);

  EamInstall *install = eam_install_new_with_scripts (priv->appid, SCRIPTDIR, ROLLBACKDIR);
  eam_install_run_async (install, cancellable, run_cb, task);
}

/*
 * eam_update_finish:
 * @trans:A #EamUpdate instance with #EamTransaction interface.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Finishes the update transaction.
 *
 * Returns: %TRUE if everything went well, %FALSE otherwise
 **/
static gboolean
eam_update_finish (EamTransaction *trans, GAsyncResult *res, GError **error)
{
  g_return_val_if_fail (EAM_IS_UPDATE (trans), FALSE);
  g_return_val_if_fail (g_task_is_valid (res, trans), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}

static void
transaction_iface_init (EamTransactionInterface *iface)
{
  iface->run_async = eam_update_run_async;
  iface->finish = eam_update_finish;
}

static void
eam_update_finalize (GObject *obj)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (EAM_UPDATE (obj));

  g_clear_pointer (&priv->appid, g_free);

  G_OBJECT_CLASS (eam_update_parent_class)->finalize (obj);
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
eam_update_class_init (EamUpdateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eam_update_finalize;
  object_class->get_property = eam_update_get_property;
  object_class->set_property = eam_update_set_property;

  /**
   * EamUpdate:appid:
   *
   * The application ID to update.
   */
  g_object_class_install_property (object_class, PROP_APPID,
    g_param_spec_string ("appid", "App ID", "Application ID", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
eam_update_init (EamUpdate *self)
{
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
  return g_object_new (EAM_TYPE_UPDATE, "appid", appid, NULL);
}
