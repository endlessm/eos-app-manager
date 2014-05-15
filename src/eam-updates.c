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
  EamPkgdb *avails;
  GList *installs, *updates;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamUpdates, eam_updates, G_TYPE_OBJECT)

G_DEFINE_QUARK (eam-updates-error-quark, eam_updates_error)

enum {
  AVAILABLE_APPS_CHANGED,
  UPDATES_FILTERED,
  SIGNAL_MAX
};

guint signals[SIGNAL_MAX];

static void
eam_updates_reset_lists (EamUpdates *self)
{
  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);

  /* this list points to structures that belong to priv->avails, hence
   * we don't delete its content */
  g_clear_pointer (&priv->installs, g_list_free);

  /* this list points to structures that belong to EamPkgdb, hence we
   * don't delete its content */
  g_clear_pointer (&priv->updates, g_list_free);

  g_clear_object (&priv->avails);
}

static void
eam_updates_finalize (GObject *obj)
{
  EamUpdatesPrivate *priv = eam_updates_get_instance_private (EAM_UPDATES (obj));

  g_object_unref (priv->wc);
  eam_updates_reset_lists (EAM_UPDATES (obj));

  G_OBJECT_CLASS (eam_updates_parent_class)->finalize (obj);
}

static void
eam_updates_class_init (EamUpdatesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eam_updates_finalize;

  signals[AVAILABLE_APPS_CHANGED] = g_signal_new ("available-apps-changed",
    G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
    g_cclosure_marshal_generic, G_TYPE_NONE, 0);

  signals[UPDATES_FILTERED] = g_signal_new ("updates-filtered",
    G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
    g_cclosure_marshal_generic, G_TYPE_NONE, 0);
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
 *
 * Returns: a new #EamUpdates instance.
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

  /* is it enough? */
  if (!g_network_monitor_get_network_available (g_network_monitor_get_default ())) {
    g_task_report_new_error (self, callback, data, eam_updates_fetch_async,
      EAM_UPDATES_ERROR, EAM_UPDATES_ERROR_NO_NETWORK,
      _("Networking is not available"));
    return;
  }

  gchar *uri = eam_rest_build_uri (EAM_REST_API_V1_GET_ALL_UPDATES, NULL);
  if (!uri) {
    g_task_report_new_error (self, callback, data, eam_updates_fetch_async,
      EAM_UPDATES_ERROR, EAM_UPDATES_ERROR_PROTOCOL_ERROR,
      _("Not valid method or protocol version"));
    return;
  }

  GTask *task = g_task_new (self, cancellable, callback, data);
  gchar *filename = build_updates_filename ();
  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);
  eam_wc_request_with_headers_async (priv->wc, uri, filename, cancellable,
    request_cb, task, "Accept", "application/json", NULL);

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
  if (node) {
    EamPkgVersion *posver = eam_pkg_version_new_from_string (
      json_node_get_string (node));
    EamPkgVersion *osver = eam_pkg_version_new_from_string (
      eam_os_get_version ());

    gboolean rel = eam_pkg_version_relate (posver, EAM_RELATION_GE, osver);
    eam_pkg_version_free (posver);
    eam_pkg_version_free (osver);

    if (!rel)
      return FALSE;
  }

  node = json_object_get_member (json, "personality");
  if (node) {
    const gchar *personality = json_node_get_string (node);
    return g_strcmp0 (personality, eam_os_get_personality ()) == 0;
  }

  return TRUE; /* if it doesn't have version nor personality, let's
                * assume it's ours. */
}

static void
foreach_json (JsonArray *array, guint index, JsonNode *node, gpointer data)
{
  EamPkgdb *avails = data;

  if (!JSON_NODE_HOLDS_OBJECT (node))
    return;

  JsonObject *json = json_node_get_object (node);
  if (!pkg_json_is_valid (json))
    return;

  EamPkg *pkg = eam_pkg_new_from_json_object (json, NULL);
  if (!pkg)
    return;

  if (!eam_pkgdb_replace (avails, pkg))
    eam_pkg_free (pkg);
}

static void
replace_avails_db (EamUpdates *self, EamPkgdb *new_avails)
{
  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);
  gboolean equal = eam_pkgdb_equal (priv->avails, new_avails);

  eam_updates_reset_lists (self);
  priv->avails = new_avails;

  if (!equal)
    g_signal_emit (self, signals[AVAILABLE_APPS_CHANGED], 0);
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

  if (!JSON_NODE_HOLDS_ARRAY (root))
    goto bail;

  JsonArray *array = json_node_get_array (root);

  EamPkgdb *avails = eam_pkgdb_new ();
  json_array_foreach_element (array, foreach_json, avails);

  replace_avails_db (self, avails);

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
 *     { app 1 data },
 *     ...
 *     { app N data }
 * |]
 *
 * @TODO: make this async
 *
 * Returns: %TRUE if updates were parsed correctly.
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

  g_clear_pointer (&priv->installs, g_list_free);
  g_clear_pointer (&priv->updates, g_list_free);

  EamPkg *apkg;
  while (eam_pkgdb_iter_next (priv->avails, &apkg)) {
    if (!apkg)
      continue;

    const EamPkg *fpkg = eam_pkgdb_get (db, eam_pkg_get_id (apkg));
    if (!fpkg) {
      priv->installs = g_list_prepend (priv->installs, apkg);
    } else {
      if (eam_pkg_version_relate (eam_pkg_get_version (apkg), EAM_RELATION_GT,
                                  eam_pkg_get_version (fpkg)))
        priv->updates = g_list_prepend (priv->updates, (gpointer) apkg);
    }
  }

  g_signal_emit (self, signals[UPDATES_FILTERED], 0);
}

/**
 * eam_updates_get_installables:
 * @self: a #EamUpdates instance.
 *
 * Don't modify this list!
 *
 * Returns: (transfer none) (element-type EamPkg): return a #GList
 * with the installable packages.
 **/
const GList *
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
 * Returns: (transfer none) (element-type EamPkg): returns a #GList
 * with the upgradable packages.
 **/
const GList *
eam_updates_get_upgradables (EamUpdates *self)
{
  g_return_val_if_fail (EAM_IS_UPDATES (self), NULL);

  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);
  return priv->updates;
}

/**
 * eam_updates_pkg_is_upgradable:
 * @self: an #EamUpdates instance
 * @appid: the application ID to update
 *
 * Checks if an update exists for the specified app ID.
 *
 * Returns: %TRUE if a new version of the application exists, %FALSE
 * otherwise
 **/
gboolean
eam_updates_pkg_is_upgradable (EamUpdates *self, const gchar *appid)
{
  g_return_val_if_fail (EAM_IS_UPDATES (self), FALSE);
  g_return_val_if_fail (appid, FALSE);

  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);
  GList *l;

  for (l = priv->updates; l; l = l->next) {
    EamPkg *pkg = l->data;
    if (g_strcmp0 (eam_pkg_get_id (pkg), appid) == 0)
      return TRUE;
  }

  return FALSE;
}

/**
 * eam_updates_pkg_is_installable:
 * @self: an #EamUpdates instance
 * @appid: the application ID to search
 *
 * Searches in the installable application list the specified app ID.
 *
 * Returns: The #EamPkg found, or %NULL if not found
 **/
const EamPkg *
eam_updates_pkg_is_installable (EamUpdates *self, const gchar *appid)
{
  g_return_val_if_fail (EAM_IS_UPDATES (self), FALSE);
  g_return_val_if_fail (appid, FALSE);

  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);
  GList *l;

  for (l = priv->installs; l; l = l->next) {
    EamPkg *pkg = l->data;
    if (g_strcmp0 (eam_pkg_get_id (pkg), appid) == 0)
      return pkg;
  }

  return NULL;
}
