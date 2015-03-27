/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "eam-log.h"
#include "eam-error.h"
#include "eam-update.h"
#include "eam-spawner.h"
#include "eam-wc.h"
#include "eam-config.h"
#include "eam-os.h"
#include "eam-version.h"
#include "eam-utils.c"

#define UPDATE_SCRIPTDIR "update/full"
#define UPDATE_ROLLBACKDIR "update/rollback"
#define XDELTA_UPDATE_SCRIPTDIR "update/xdelta"
#define XDELTA_UPDATE_ROLLBACKDIR "update/rollback"

#define UPDATE_BUNDLE_EXT ".xdelta"
#define INSTALL_BUNDLE_EXT ".bundle"

typedef enum {
  EAM_ACTION_UPDATE,        /* Update downloading the complete bundle */
  EAM_ACTION_XDELTA_UPDATE, /* Update applying xdelta diff files */
} EamAction;

typedef struct _EamUpdatePrivate        EamUpdatePrivate;

struct _EamUpdatePrivate
{
  gchar *appid;
  gboolean allow_deltas;
  gchar *from_version;
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
    priv->action = EAM_ACTION_UPDATE;

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

static const char *
get_extension (EamUpdate *self)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  if (priv->action == EAM_ACTION_XDELTA_UPDATE)
    return UPDATE_BUNDLE_EXT;

  return INSTALL_BUNDLE_EXT;
}

static gchar *
build_tarball_filename (EamUpdate *self)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  return eam_utils_build_tarball_filename (priv->bundle_location, priv->appid,
                                           get_extension (self));
}

static const gchar *
get_scriptdir (EamUpdate *self)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  switch (priv->action) {
  case EAM_ACTION_UPDATE:
    return UPDATE_SCRIPTDIR;
  case EAM_ACTION_XDELTA_UPDATE:
    return XDELTA_UPDATE_SCRIPTDIR;
  default:
    g_assert_not_reached ();
  }
}

static void
run_scripts (EamUpdate *self, const gchar *scriptdir,
  GCancellable *cancellable, GAsyncReadyCallback callback, GTask *task)
{
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  eam_utils_run_bundle_scripts (priv->appid,
                                build_tarball_filename (self),
                                scriptdir,
                                priv->bundle_location != NULL, /* external download? */
                                cancellable,
                                callback,
                                task);
}

static void
rollback (GTask *task, GError *error)
{
  EamUpdate *self = EAM_UPDATE (g_task_get_source_object (task));
  EamUpdatePrivate *priv = eam_update_get_instance_private (self);

  /* Log some messages about what happened */
  switch (priv->action) {
  case EAM_ACTION_XDELTA_UPDATE:
    eam_log_error_message ("Incremental update failed: %s", error->message);
    break;
  case EAM_ACTION_UPDATE:
    eam_log_error_message ("Full update failed: %s", error->message);
    break;
  }

  /* Rollback action if possible */
  if (priv->action == EAM_ACTION_XDELTA_UPDATE) {
    eam_log_error_message ("Can't rollback xdelta update - bundle not present.");
  }

  /* Clean up */
  g_task_return_error (task, error);
  g_object_unref (task);
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

  if (priv->bundle_location == NULL) {
    g_task_return_new_error (task, EAM_ERROR, EAM_ERROR_INVALID_FILE,
                             "No bundle location set");
    g_object_unref (task);
    return;
  }

  run_scripts (self, get_scriptdir (self), cancellable, action_cb, task);
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
