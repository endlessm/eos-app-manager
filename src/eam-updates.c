/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include <glib/gi18n.h>

#include "eam-error.h"
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

enum {
  AVAILABLE_APPS_CHANGED,
  SIGNAL_MAX
};

static guint signals[SIGNAL_MAX];

static void
eam_updates_reset_lists (EamUpdates *self)
{
  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);

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
}

static void
eam_updates_init (EamUpdates *self)
{
  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);

  priv->wc = eam_wc_new ();
  priv->avails = eam_pkgdb_new ();
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
build_updates_filename (void)
{
  return g_build_filename (eam_config_dldir (), "updates.json", NULL);
}

typedef struct {
  GTask *task;
  GCancellable *cancellable;
  GFile *target_file;
} RequestData;

static void
request_data_free (gpointer data_)
{
  if (G_UNLIKELY (data_ == NULL))
    return;

  RequestData *data = data_;

  g_object_unref (data->target_file);
  g_object_unref (data->task);

  g_free (data);
}

static void
request_cb (GObject *source, GAsyncResult *result, gpointer data_)
{
  RequestData *data = data_;
  GError *error = NULL;
  GInputStream *istream = NULL;
  GFileOutputStream *ostream = NULL;
  gssize size;

  istream = eam_wc_request_instream_finish (EAM_WC (source), result, &error);
  if (error) {
    g_task_return_error (data->task, error);
    goto bail;
  }

  if (g_task_return_error_if_cancelled (data->task))
    goto bail;

  ostream = g_file_replace (data->target_file, NULL, FALSE, 
                            G_FILE_CREATE_REPLACE_DESTINATION,
                            data->cancellable, &error);
  if (error) {
    g_task_return_error (data->task, error);
    goto bail;
  }

  size = g_output_stream_splice (G_OUTPUT_STREAM (ostream), istream,
    G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
    data->cancellable, &error);
  if (error) {
    g_task_return_error (data->task, error);
    goto bail;
  }

  g_task_return_int (data->task, size);

bail:
  if (istream)
    g_object_unref (istream);
  if (ostream)
    g_object_unref (ostream);
  request_data_free (data);
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
      EAM_ERROR, EAM_ERROR_NO_NETWORK,
      _("Networking is not available"));
    return;
  }

  gchar *uri = eam_rest_build_uri (EAM_REST_API_V1_GET_ALL_UPDATES, NULL);
  if (!uri) {
    g_task_report_new_error (self, callback, data, eam_updates_fetch_async,
      EAM_ERROR, EAM_ERROR_PROTOCOL_ERROR,
      _("Not valid method or protocol version"));
    return;
  }

  char *target_file = build_updates_filename ();
  RequestData *clos = g_new0 (RequestData, 1);

  clos->target_file = g_file_new_for_path (target_file);
  clos->task = g_task_new (self, cancellable, callback, data);
  clos->cancellable = cancellable;

  EamUpdatesPrivate *priv = eam_updates_get_instance_private (self);
  eam_wc_request_instream_with_headers_async (priv->wc, uri,
                                              clos->cancellable,
                                              request_cb, clos,
                                              "Accept", "application/json",
                                              NULL);

  g_free (target_file);
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

static void
foreach_json (JsonArray *array, guint index, JsonNode *node, gpointer data)
{
  EamPkgdb *avails = data;

  if (!JSON_NODE_HOLDS_OBJECT (node))
    return;

  JsonObject *json = json_node_get_object (node);
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
  g_set_error (error, EAM_ERROR, EAM_ERROR_INVALID_FILE,
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
