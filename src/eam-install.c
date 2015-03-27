/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include <glib/gi18n.h>

#include "eam-log.h"
#include "eam-error.h"
#include "eam-install.h"
#include "eam-spawner.h"
#include "eam-wc.h"
#include "eam-config.h"
#include "eam-os.h"
#include "eam-version.h"
#include "eam-utils.h"

#define INSTALL_SCRIPTDIR "install/run"
#define INSTALL_ROLLBACKDIR "install/rollback"
#define INSTALL_BUNDLE_EXT ".bundle"

typedef struct _EamInstallPrivate        EamInstallPrivate;

struct _EamInstallPrivate
{
  gchar *appid;
  gchar *from_version;
  char *bundle_location;
};

static void transaction_iface_init (EamTransactionInterface *iface);
static void eam_install_run_async (EamTransaction *trans, GCancellable *cancellable,
   GAsyncReadyCallback callback, gpointer data);
static gboolean eam_install_finish (EamTransaction *trans, GAsyncResult *res,
   GError **error);

G_DEFINE_TYPE_WITH_CODE (EamInstall, eam_install, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (EAM_TYPE_TRANSACTION, transaction_iface_init)
  G_ADD_PRIVATE (EamInstall));

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
eam_install_set_property (GObject *obj, guint prop_id, const GValue *value,
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
eam_install_get_property (GObject *obj, guint prop_id, GValue *value,
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

static gchar *
build_tarball_filename (EamInstall *self)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

  return eam_utils_build_tarball_filename (priv->bundle_location, priv->appid,
                                           INSTALL_BUNDLE_EXT);
}

static const gchar *
get_rollback_scriptdir (EamInstall *self)
{
  return INSTALL_ROLLBACKDIR;
}

static const gchar *
get_scriptdir (EamInstall *self)
{
  return INSTALL_SCRIPTDIR;
}

static void
run_scripts (EamInstall *self, const gchar *scriptdir,
  GCancellable *cancellable, GAsyncReadyCallback callback, GTask *task)
{
  EamInstallPrivate *priv = eam_install_get_instance_private (self);

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
  EamInstall *self = EAM_INSTALL (g_task_get_source_object (task));

  /* Log some messages about what happened */
  eam_log_error_message ("Install failed: %s", error->message);

  /* Rollback action if possible */
  /* Use cancellable == NULL as we are returning immediately after
     using g_task_return. */
  run_scripts (self, get_rollback_scriptdir (self), NULL, NULL, NULL);

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

/**
 * 1. Download the update link
 * 1. Download the application and its sha256sum
 * 2. Set the envvars
 * 3. Run the scripts
 * 4. Rollback if something fails
 */
static void
eam_install_run_async (EamTransaction *trans, GCancellable *cancellable,
  GAsyncReadyCallback callback, gpointer data)
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

  run_scripts (self, get_scriptdir (self), cancellable, action_cb, task);
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
