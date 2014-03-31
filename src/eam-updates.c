/* Copyright 2014 Endless Mobile, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>

#include "eam-updates.h"
#include "eam-wc.h"
#include "eam-rest.h"
#include "eam-config.h"
#include "eam-pkg.h"
#include "eam-os.h"
#include "eam-version.h"

typedef struct _EamUpdatesPrivate EamUpdatesPrivate;

struct _EamUpdatesPrivate
{
  EamWc *wc;
  GList *avails;
  GList *installs, *updates;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamUpdates, eam_updates, G_TYPE_OBJECT)

G_DEFINE_QUARK (eam-updates-error-quark, eam_updates_error)

static void
eam_updates_finalize (GObject *obj)
{
  EamUpdatesPrivate *priv = eam_updates_get_instance_private (EAM_UPDATES (obj));

  g_object_unref (priv->wc);

  /* this list points to structures that belong to priv->avails, hence
   * we don't delete its content */
  if (priv->installs)
    g_list_free (priv->installs);

  /* this list points to structures that belong to EamPkgdb, hence we
   * don't delete its content */
  if (priv->updates)
    g_list_free (priv->updates);

  if (priv->avails)
    g_list_free_full (priv->avails, eam_pkg_free);

  G_OBJECT_CLASS (eam_updates_parent_class)->finalize (obj);
}

static void
eam_updates_class_init (EamUpdatesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eam_updates_finalize;
}

static void
eam_updates_init (EamUpdates *self)
{
  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);

  priv->wc = eam_wc_new ();
}

/**
 * eam_updates_new:
 *
 * Create a new instance of #EamUpdates with the default appdir.
 */
EamUpdates *
eam_updates_new ()
{
  return g_object_new (EAM_TYPE_UPDATES, NULL);
}

static gchar *
build_updates_filename ()
{
  EamConfig *cfg = eam_config_get ();
  return g_build_filename (cfg->dldir, "updates.json", NULL);
}

static void
request_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GTask *task = data;
  GError *error = NULL;

  gssize size = eam_wc_request_finish (EAM_WC (source), result, &error);
  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  if (g_task_return_error_if_cancelled (task))
    goto bail;

  g_task_return_int (task, size);

bail:
  g_object_unref (task);
}

/**
 * eam_updates_fetch_async:
 * @self: a #EamUpdates instance
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore
 * @callback: (scope async): callback to call when the request is satisfied
 * @data: (closure): the data to pass to callback function
 *
 * Downloads the updates list and stores it in the donwload directory.
 **/
void
eam_updates_fetch_async (EamUpdates *self, GCancellable *cancellable,
  GAsyncReadyCallback callback, gpointer data)
{
  g_return_if_fail (EAM_IS_UPDATES (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);

  GTask *task = g_task_new (self, cancellable, callback, data);

  gchar *uri = eam_rest_build_uri (EAM_REST_API_V1_GET_ALL_UPDATES, NULL);
  if (!uri) {
    g_task_return_new_error (task, EAM_UPDATES_ERROR,
      EAM_UPDATES_ERROR_PROTOCOL_ERROR, _("Not valid method or protocol version"));
    g_object_unref (task);
    return;
  }

  gchar *filename = build_updates_filename ();
  eam_wc_request_async (priv->wc, uri, filename, cancellable, request_cb, task);

  g_free (filename);
  g_free (uri);
}

/**
 * eam_updates_dl_updates_finish:
 * @self:A #EamUpdates instance
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Finishes a download updates operation
 *
 * Returns: a #gssize containing the number of bytes downloaded.
 **/
gssize
eam_updates_fetch_finish (EamUpdates *self, GAsyncResult *result,
  GError **error)
{
  g_return_val_if_fail (EAM_IS_UPDATES (self), -1);

  g_return_val_if_fail (g_task_is_valid (result, self), -1);
  return g_task_propagate_int (G_TASK (result), error);
}

static inline gboolean
pkg_json_is_valid (JsonObject *json)
{
  JsonNode *node = json_object_get_member (json, "minOsVersion");
  if (!node)
    return TRUE; /* if it doesn't have version, let's assume it's ours. */

  const gchar *osver = json_node_get_string (node);
  return g_strcmp0 (osver, eam_os_get_version ()) == 0;
}

static void
foreach_json (JsonArray *array, guint index, JsonNode *node, gpointer data)
{
  EamUpdates *self = data;

  if (!JSON_NODE_HOLDS_OBJECT (node))
    return;

  JsonObject *json = json_node_get_object (node);
  if (!pkg_json_is_valid (json))
    return;

  EamPkg *pkg = eam_pkg_new_from_json_object (json);
  if (!pkg)
    return;

  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);
  priv->avails = g_list_prepend (priv->avails, pkg);
}

/**
 * eam_updates_load:
 * @self: a #EamUpdates instance
 * @root: a #JsonNode root node
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Given a root node, the method will look for a array node with the updates.
 *
 * Returns: %TRUE if update packages were found.
 **/
gboolean
eam_updates_load (EamUpdates *self, JsonNode *root, GError **error)
{
  g_return_val_if_fail (EAM_IS_UPDATES (self), FALSE);
  g_return_val_if_fail (root, FALSE);

  JsonArray *array = NULL;

  if (JSON_NODE_HOLDS_ARRAY (root))
    array = json_node_get_array (root);
  else if (JSON_NODE_HOLDS_OBJECT (root)) {
    JsonObject *json = json_node_get_object (root);
    GList *values = json_object_get_values (json);
    if (JSON_NODE_HOLDS_ARRAY (values->data))
      array = json_node_get_array (values->data);
    g_list_free (values);
  }

  if (!array)
    goto bail;

  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);

  if (priv->avails) {
    g_list_free_full (priv->avails, eam_pkg_free);
    priv->avails = NULL;
  }

  json_array_foreach_element (array, foreach_json, self);

  if (g_list_length (priv->avails) == 0)
    goto bail;

  return TRUE;

bail:
  g_set_error (error, EAM_UPDATES_ERROR, EAM_UPDATES_ERROR_INVALID_FILE,
     _("Not valid file with updates list"));

  return FALSE;
}

/**
 * eam_updates_parse:
 * @self: an #EamUpdates instance
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Expected format:
 *
 * |[
 * {
 *   [
 *     { app 1 data },
 *     ...
 *     { app N data }
 *   ]
 * }
 * |]
 *
 * @TODO: make this async
 *
 * Returns: %TRUE if updates were parser correctly.
 **/
gboolean
eam_updates_parse (EamUpdates *self, GError **error)
{
  g_return_val_if_fail (EAM_IS_UPDATES (self), FALSE);

  JsonParser *parser = json_parser_new ();
  gchar *file = build_updates_filename ();

  gboolean ret = json_parser_load_from_file (parser, file, error);
  g_free (file);
  if (!ret)
    goto bail;

  JsonNode *root = json_parser_get_root (parser);
  ret = eam_updates_load (self, root, error);

bail:
  g_object_unref (parser);

  return ret;
}

/**
 * eam_updates_filter:
 * @self: a #EamUpdates instance
 * @db: a #EamPkgdb instance
 *
 * This method will compare the available list of packages with the
 * installed package database. Two list will be generated: the
 * installable package list and the upgradable package list.
 *
 * @TODO: filter the obsolete package (installed but not available)
 **/
void
eam_updates_filter (EamUpdates *self, EamPkgdb *db)
{
  g_return_if_fail (EAM_IS_UPDATES (self));

  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);

  GList *l;
  for (l = priv->avails; l && l->data; l = l->next) {
    EamPkg *apkg = l->data;
    EamPkg *fpkg = eam_pkgdb_get (db, apkg->id);
    if (!fpkg) {
      priv->installs = g_list_prepend (priv->installs, apkg);
    } else {
      if (eam_pkg_version_relate (apkg->version, EAM_RELATION_GT, fpkg->version))
        priv->updates = g_list_prepend (priv->updates, fpkg);
    }
  }
}

/**
 * eam_updates_get_installables:
 * @self: a #EamUpdates instance.
 *
 * Don't modify this list!
 *
 * Returns: (transfer none): return a #GList with the installable
 * packages.
 **/
GList *
eam_updates_get_installables (EamUpdates *self)
{
  g_return_val_if_fail (EAM_IS_UPDATES (self), NULL);

  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);
  return priv->installs;
}

/**
 * eam_updates_get_upgradables:
 * @self: a #EamUpdates instance.
 *
 * Don't modify this list!
 *
 * Returns: (transfer none): returns a #GList with the upgradable
 * packages.
 **/
GList *
eam_updates_get_upgradables (EamUpdates *self)
{
  g_return_val_if_fail (EAM_IS_UPDATES (self), NULL);

  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);
  return priv->updates;
}
