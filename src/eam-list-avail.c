/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "eam-list-avail.h"

typedef struct _EamListAvailPrivate EamListAvailPrivate;

struct _EamListAvailPrivate
{
  EamPkgdb *db;
  EamUpdates *updates;
  gboolean dbreloaded;

  char *language;
};

enum
{
  PROP_DBRELOADED = 1,
  PROP_PKGDB,
  PROP_UPDATES,
  PROP_LANGUAGE
};

static void transaction_iface_init (EamTransactionInterface *iface);
static void eam_list_avail_run_async (EamTransaction *trans, GCancellable *cancellable,
  GAsyncReadyCallback callback, gpointer data);
static gboolean eam_list_avail_finish (EamTransaction *trans, GAsyncResult *res,
  GError **error);

G_DEFINE_TYPE_WITH_CODE (EamListAvail, eam_list_avail, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (EAM_TYPE_TRANSACTION, transaction_iface_init)
  G_ADD_PRIVATE (EamListAvail));

static void
transaction_iface_init (EamTransactionInterface *iface)
{
  iface->run_async = eam_list_avail_run_async;
  iface->finish = eam_list_avail_finish;
}

static void
eam_list_avail_finalize (GObject *obj)
{
  EamListAvailPrivate *priv = eam_list_avail_get_instance_private (EAM_LIST_AVAIL (obj));

  g_clear_object (&priv->db);
  g_clear_object (&priv->updates);

  g_free (priv->language);

  G_OBJECT_CLASS (eam_list_avail_parent_class)->finalize (obj);
}

static void
eam_list_avail_set_property (GObject *obj, guint prop_id, const GValue *value,
  GParamSpec *pspec)
{
  EamListAvailPrivate *priv = eam_list_avail_get_instance_private (EAM_LIST_AVAIL (obj));

  switch (prop_id) {
  case PROP_DBRELOADED:
    priv->dbreloaded = g_value_get_boolean (value);
    break;
  case PROP_PKGDB:
    priv->db = g_value_dup_object (value);
    break;
  case PROP_UPDATES:
    priv->updates = g_value_dup_object (value);
    break;
  case PROP_LANGUAGE:
    priv->language = g_value_dup_string (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
eam_list_avail_get_property (GObject *obj, guint prop_id, GValue *value,
  GParamSpec *pspec)
{
  EamListAvailPrivate *priv = eam_list_avail_get_instance_private (EAM_LIST_AVAIL (obj));

  switch (prop_id) {
  case PROP_DBRELOADED:
    g_value_set_boolean (value, priv->dbreloaded);
    break;
  case PROP_PKGDB:
    g_value_set_object (value, priv->db);
    break;
  case PROP_UPDATES:
    g_value_set_object (value, priv->updates);
    break;
  case PROP_LANGUAGE:
    g_value_set_string (value, priv->language);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
eam_list_avail_class_init (EamListAvailClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eam_list_avail_finalize;
  object_class->get_property = eam_list_avail_get_property;
  object_class->set_property = eam_list_avail_set_property;

  /**
   * EamListAvail:dbreloaded:
   *
   * Whether the package database was reloaded
   */
  g_object_class_install_property (object_class, PROP_DBRELOADED,
    g_param_spec_boolean ("dbreloaded", "DB reloaded",
      "The package database was reloaded", FALSE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * EamListAvail:pkgdb:
   *
   * The package database
   */
  g_object_class_install_property (object_class, PROP_PKGDB,
    g_param_spec_object ("pkgdb", "Pkg DB", "Packages Database", EAM_TYPE_PKGDB,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * EamListAvail:updates:
   *
   * The #EamUpdates to use
   */
  g_object_class_install_property (object_class, PROP_UPDATES,
    g_param_spec_object ("updates", "updates-manager", "", EAM_TYPE_UPDATES,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_LANGUAGE,
    g_param_spec_string ("language", "Language", "Language used to filter packages", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}


static void
eam_list_avail_init (EamListAvail *self)
{
}

/**
 * eam_list_avail_new:
 * @dbreloaded: %TRUE if the @db was just reloaded
 * @db: a #EamPkgdb instance
 * @updates: a #Eampupdates instance
 *
 * NOTE: This is a dummy async transaction. Actually it is not async
 * at all, but we do keep the async transaction semantics for
 * code readability.
 *
 * Returns: a new #EamListAvail instance with #EamTransaction interface.
 **/
EamTransaction *
eam_list_avail_new (gboolean dbreloaded, EamPkgdb *db, EamUpdates *updates, const char *language)
{
  return g_object_new (EAM_TYPE_LIST_AVAIL, "dbreloaded", dbreloaded,
    "pkgdb", db, "updates", updates, "language", language, NULL);
}

static void
eam_list_avail_run_async (EamTransaction *trans, GCancellable *cancellable,
  GAsyncReadyCallback callback, gpointer data)
{
  g_return_if_fail (EAM_IS_LIST_AVAIL (trans));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  EamListAvail *self = EAM_LIST_AVAIL (trans);
  EamListAvailPrivate *priv = eam_list_avail_get_instance_private (self);
  g_assert (priv->updates);

  if (priv->dbreloaded) {
    g_assert (priv->db);
    eam_updates_filter (priv->updates, priv->db, priv->language);
  }

  GTask *task = g_task_new (self, cancellable, callback, data);
  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

static gboolean
eam_list_avail_finish (EamTransaction *trans, GAsyncResult *res, GError **error)
{
  g_return_val_if_fail (EAM_IS_LIST_AVAIL (trans), FALSE);
  g_return_val_if_fail (g_task_is_valid (res, EAM_LIST_AVAIL (trans)), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}
