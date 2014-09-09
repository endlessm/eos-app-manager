/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include "eam-refresh.h"

typedef struct _EamRefreshPrivate	EamRefreshPrivate;

struct _EamRefreshPrivate
{
  EamPkgdb *db;
  EamUpdates *updates;
};

static void transaction_iface_init (EamTransactionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EamRefresh, eam_refresh, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (EAM_TYPE_TRANSACTION, transaction_iface_init)
  G_ADD_PRIVATE (EamRefresh));

enum
{
  PROP_PKGDB = 1,
  PROP_UPDATES
};

static void
fetch_updates_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  GTask *task = data;
  EamRefresh *self = g_task_get_source_object (task);
  EamRefreshPrivate *priv = eam_refresh_get_instance_private (self);
  GError *error = NULL;

  eam_updates_fetch_finish (EAM_UPDATES (source), res, &error);
  if (error) {
    g_task_return_error (task, error);
    goto out;
  }

  if (g_task_return_error_if_cancelled (task))
    goto out;

  eam_updates_parse (priv->updates, &error);
  if (error) {
    g_task_return_error (task, error);
    goto out;
  }

  eam_updates_filter (priv->updates, priv->db, NULL);

  g_task_return_boolean (task, TRUE);

out:
  g_object_unref (task);
}

/**
 * eam_refresh_run_async:
 * @trans: a #EamRefresh intance with #EamTransaction interface.
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore
 * @callback: (scope async): callback to call when the request is satisfied
 * @data: (closure): the data to pass to callback function
 *
 * Lanch the refresh method
 *
 * 1. fetch the available package list
 * 2. parse the available package list
 * 3. filter the available package list
 **/
static void
eam_refresh_run_async (EamTransaction *trans, GCancellable *cancellable,
   GAsyncReadyCallback callback, gpointer data)
{
  g_return_if_fail (EAM_IS_REFRESH (trans));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  EamRefresh *self = EAM_REFRESH (trans);
  EamRefreshPrivate *priv = eam_refresh_get_instance_private (self);
  g_assert (priv->db);

  GTask *task = g_task_new (self, cancellable, callback, data);
  eam_updates_fetch_async (priv->updates, cancellable, fetch_updates_cb, task);
}

/*
 * eam_refresh_finish:
 * @trans:A #EamUpdates instance with #EamTransaction interface.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Finishes the refresh transaction
 *
 * Returns: %TRUE if everything went well
 **/
static gboolean
eam_refresh_finish (EamTransaction *trans, GAsyncResult *res, GError **error)
{
  g_return_val_if_fail (EAM_IS_REFRESH (trans), FALSE);
  g_return_val_if_fail (g_task_is_valid (res, EAM_REFRESH (trans)), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}

static void
transaction_iface_init (EamTransactionInterface *iface)
{
  iface->run_async = eam_refresh_run_async;
  iface->finish = eam_refresh_finish;
}

static void
eam_refresh_finalize (GObject *obj)
{
  EamRefreshPrivate *priv = eam_refresh_get_instance_private (EAM_REFRESH (obj));

  g_clear_object (&priv->db);
  g_clear_object (&priv->updates);

  G_OBJECT_CLASS (eam_refresh_parent_class)->finalize (obj);
}

static void
eam_refresh_set_property (GObject *obj, guint prop_id, const GValue *value,
  GParamSpec *pspec)
{
  EamRefreshPrivate *priv = eam_refresh_get_instance_private (EAM_REFRESH (obj));

  switch (prop_id) {
  case PROP_PKGDB:
    priv->db = g_value_dup_object (value);
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
eam_refresh_get_property (GObject *obj, guint prop_id, GValue *value,
  GParamSpec *pspec)
{
  EamRefreshPrivate *priv = eam_refresh_get_instance_private (EAM_REFRESH (obj));

  switch (prop_id) {
  case PROP_PKGDB:
    g_value_set_object (value, priv->db);
    break;
  case PROP_UPDATES:
    g_value_set_object (value, priv->updates);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
eam_refresh_class_init (EamRefreshClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eam_refresh_finalize;
  object_class->get_property = eam_refresh_get_property;
  object_class->set_property = eam_refresh_set_property;

  /**
   * EamRefresh:pkgdb:
   *
   * The package database
   */
  g_object_class_install_property (object_class, PROP_PKGDB,
    g_param_spec_object ("pkgdb", "Pkg DB", "Packages Database", EAM_TYPE_PKGDB,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * EamRefresh:updates:
   *
   * The #EamUpdates to use
   */
  g_object_class_install_property (object_class, PROP_UPDATES,
    g_param_spec_object ("updates", "updates-manager", "", EAM_TYPE_UPDATES,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
eam_refresh_init (EamRefresh *self)
{
}

/**
 * eam_refresh_new:
 * @db: a #EamPkgdb instance.
 * @updates: a #EamUpdates intance.
 *
 * Returns: a new instance of #EamRefresh with #EamTransaction interface.
 */
EamTransaction *
eam_refresh_new (EamPkgdb *db, EamUpdates *updates)
{
  return EAM_TRANSACTION (g_object_new (EAM_TYPE_REFRESH, "pkgdb", db,
    "updates", updates, NULL));
}
