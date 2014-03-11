/* Copyright 2014 Endless Mobile, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <libsoup/soup.h>
#include <glib/gi18n.h>

#include "eam-wc.h"
#include "eam-wc-file.h"

typedef struct _EamWcPrivate EamWcPrivate;

struct _EamWcPrivate
{
  SoupSession *session;
  SoupLoggerLogLevel level;
  gchar *filename;
  EamWcFile *file;
  gulong phnd; /* file progress signal handle */
};

G_DEFINE_TYPE_WITH_PRIVATE (EamWc, eam_wc, G_TYPE_OBJECT)

enum {
  PROP_LOG_LEVEL = 1,
  PROP_USER_AGENT,
  PROP_FILENAME
};

enum {
  SIG_PROGRESS,
  SIG_MAX
};

static gint signals[SIG_MAX];

static void
eam_wc_reset (EamWc *self)
{
  EamWcPrivate *priv = eam_wc_get_instance_private (self);

  if (priv->phnd > 0) {
    g_signal_handler_disconnect (priv->file, priv->phnd);
    priv->phnd = 0;
  }

  if (priv->file)
    g_clear_object (&priv->file);

  g_free (priv->filename);
}

static void
eam_wc_finalize (GObject *obj)
{
  EamWcPrivate *priv = eam_wc_get_instance_private (EAM_WC (obj));

  g_object_unref (priv->session);
  eam_wc_reset (EAM_WC (obj));

  G_OBJECT_CLASS (eam_wc_parent_class)->finalize (obj);
}

static void
eam_wc_set_property (GObject *obj, guint propid, const GValue *value,
  GParamSpec *pspec)
{
  EamWc *wc = EAM_WC (obj);
  EamWcPrivate *priv = eam_wc_get_instance_private (wc);

  switch (propid) {
  case PROP_LOG_LEVEL:
    eam_wc_set_log_level (wc, g_value_get_uint (value));
    break;
  case PROP_FILENAME:
    g_free (priv->filename);
    priv->filename = g_value_dup_string (value);
    break;
  case PROP_USER_AGENT:
    g_object_set_property (G_OBJECT (priv->session), "user-agent", value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (wc, propid, pspec);
  }
}

static void
eam_wc_get_property (GObject *obj, guint propid, GValue *value,
  GParamSpec *pspec)
{
  EamWc *wc = EAM_WC (obj);
  EamWcPrivate *priv = eam_wc_get_instance_private (wc);

  switch (propid) {
  case PROP_LOG_LEVEL:
    g_value_set_uint (value, priv->level);
    break;
  case PROP_FILENAME:
    g_value_set_string (value, priv->filename);
    break;
  case PROP_USER_AGENT:
    g_object_get_property (G_OBJECT (priv->session), "user_agent", value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (wc, propid, pspec);
  }
}

static void
eam_wc_class_init (EamWcClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eam_wc_finalize;
  object_class->set_property = eam_wc_set_property;
  object_class->get_property = eam_wc_get_property;

  /**
   * EamWc::loglevel:
   *
   * The log level for HTTP connections. This value is used by libsoup.
   */
  g_object_class_install_property (object_class, PROP_LOG_LEVEL,
    g_param_spec_uint ("loglevel", "Log level", "Log level for HTTP connections",
      0, 3, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * EamWc::filename:
   *
   * The directory where the web client will store the donwloaded files.
   */
  g_object_class_install_property (object_class, PROP_FILENAME,
    g_param_spec_string ("filename", "File name", NULL,
      NULL, G_PARAM_READWRITE |  G_PARAM_CONSTRUCT |  G_PARAM_STATIC_STRINGS));

  /**
   * EamWc::user-agent:
   *
   * User agent identifier.
   */
  g_object_class_install_property (object_class, PROP_USER_AGENT,
    g_param_spec_string ("user-agent", "User Agent", "User agent identifier",
      NULL, G_PARAM_READWRITE |  G_PARAM_CONSTRUCT |  G_PARAM_STATIC_STRINGS));

  /**
   * EamWcFile::progress:
   * @self: The #EamWcFile instance
   *
   * Returns the number of bytes wrote in disk
   */
  signals[SIG_PROGRESS] = g_signal_new ("progress",
    G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION, 0,
    NULL, NULL, g_cclosure_marshal_VOID__ULONG, G_TYPE_NONE, 0);
}

static void
eam_wc_init (EamWc *wc)
{
  EamWcPrivate *priv = eam_wc_get_instance_private (wc);

  priv->session = soup_session_async_new ();
  g_object_set (priv->session, "use-thread-context", TRUE, NULL);

  priv->filename = NULL;
}

struct task_clos {
  GInputStream *instream;
  gsize length;
};

static void
task_closure_free (struct task_clos *clos)
{
  if (clos->instream)
    g_object_unref (clos->instream);
  g_slice_free (struct task_clos, clos);
}

static void
file_write_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GTask *task = data;
  GError *error = NULL;
  eam_wc_file_write_bytes_finish (EAM_WC_FILE (source), result, &error);
  if (error) {
    g_task_return_error (task, error);
    goto done;
  }

  if (g_task_return_error_if_cancelled (task))
    goto done;

  g_debug ("finished!");
  g_task_return_boolean (task, TRUE); /* we finished! */

done:
  {
    struct task_clos *clos = g_task_get_task_data (task);
    g_input_stream_close (clos->instream, NULL, NULL);
    g_object_unref (task);
  }
}

static void
read_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GTask *task = data;
  struct task_clos *clos = g_task_get_task_data (task);
  GInputStream *instream = G_INPUT_STREAM (source);

  g_assert (instream == clos->instream);

  GError *error = NULL;
  GBytes *buffer = g_input_stream_read_bytes_finish (instream, result, &error);
  if (error) {
    g_task_return_error (task, error);
    goto done;
  }

  if (g_task_return_error_if_cancelled (task))
    goto done;

  EamWc *wc = g_task_get_source_object (task);
  EamWcPrivate *priv = eam_wc_get_instance_private (wc);
  GCancellable *cancellable = g_task_get_cancellable (task);
  gsize bufsiz = g_bytes_get_size (buffer);

  if (eam_wc_file_queueing (priv->file)) {
    eam_wc_file_queue_bytes (priv->file, buffer);
  } else {
    g_debug ("write buffer async");
    eam_wc_file_write_bytes_async (priv->file, buffer, cancellable,
      file_write_cb, data);
  }

  if (bufsiz > 0) { /* we still have something to read */
    g_debug ("reading %li bytes", clos->length);
    g_input_stream_read_bytes_async (instream, clos->length, G_PRIORITY_DEFAULT,
      cancellable, read_cb, data);
  }

  return;

done:
  g_input_stream_close (clos->instream, NULL, NULL);
  g_object_unref (task);
}

static void
file_open_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  EamWcFile *file = EAM_WC_FILE (source);
  GTask *task = data;

  struct task_clos *clos = g_task_get_task_data (task);
  g_assert (clos->instream);

  GError *error = NULL;
  eam_wc_file_open_finish (file, result, &error);
  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  if (g_task_return_error_if_cancelled (task))
    goto bail;

  g_debug ("reading %li bytes", clos->length);
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_input_stream_read_bytes_async (clos->instream, clos->length,
    G_PRIORITY_DEFAULT, cancellable, read_cb, data);

  return;

bail:
    g_input_stream_close (clos->instream, NULL, NULL);
    g_object_unref (task);
    return;
}

static void
emit_progress (EamWc *self, gulong sum, gpointer data)
{
  g_signal_emit (self, signals[SIG_PROGRESS], 0, sum);
}

static void
request_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  SoupRequest *request = SOUP_REQUEST (source);
  GTask *task = data;

  GError *error = NULL;
  GInputStream *instream = soup_request_send_finish (request, result, &error);
  if (error) {
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  if (g_task_return_error_if_cancelled (task)) {
    g_object_unref (task);
    return;
  }

  struct task_clos *clos = g_task_get_task_data (task);
  clos->instream = instream;

  goffset len = soup_request_get_content_length (request);

  EamWc *wc = g_task_get_source_object (task);
  EamWcPrivate *priv = eam_wc_get_instance_private (wc);
  priv->file = eam_wc_file_new ();
  g_object_set (priv->file, "size", len, NULL);
  priv->phnd = g_signal_connect_swapped (priv->file, "progress",
    G_CALLBACK (emit_progress), wc);

  /* chunks of 8K as webkitgtk+ does */
  clos->length = (len > 0 && len < 8192) ? len : 8192;
  g_debug ("Downlading %li bytes in chunks of %li bytes", len, clos->length);

  GCancellable *cancellable = g_task_get_cancellable (task);
  eam_wc_file_open_async (priv->file, priv->filename, cancellable, file_open_cb, data);
}

/**
 * eam_wc_request_async:
 * @self: a #EamWc instance
 * @uri: The URI of the resource to request
 * @cancellable: (allow-none): a #GCancellable instance or %NULL to ignore
 * @callback: The callback when the result is ready
 * @data: User data set for the @callback
 *
 * Request the fetching of a web resource given the @uri. This request is
 * asynchronous, thus the result will be returned within the @callback.
 */
void
eam_wc_request_async (EamWc *self, const char *uri, GCancellable *cancellable,
  GAsyncReadyCallback callback, gpointer data)
{
  eam_wc_request_with_headers_hash_async (self, uri, NULL, cancellable,
    callback, data);
}

/**
 * eam_wc_request_with_headers_async:
 * @self: a #EamWc instance
 * @uri: The URI of the resource to request
 * @cancellable: (allow-none): a #GCancellable instance or %NULL to ignore
 * @callback: The callback when the result is ready
 * @data: User data set for the @callback
 * @...: List of tuples of header name and header value, terminated by
 * %NULL.
 *
 * Request the fetching of a web resource given the @uri. This request is
 * asynchronous, thus the result will be returned within the @callback.
 */
void eam_wc_request_with_headers_async (EamWc *self, const char *uri,
  GCancellable *cancellable, GAsyncReadyCallback callback, gpointer data, ...)
{
  va_list va_args;
  const gchar *header_name = NULL, *header_value = NULL;
  GHashTable *headers = NULL;

  va_start (va_args, data);

  header_name = va_arg (va_args, const gchar *);
  while (header_name) {
    header_value = va_arg (va_args, const gchar *);
    if (header_value) {
      if (headers == NULL) {
        headers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
      }
      g_hash_table_insert (headers, g_strdup (header_name), g_strdup (header_value));
    }
    header_name = va_arg (va_args, const gchar *);
  }

  va_end (va_args);

  eam_wc_request_with_headers_hash_async (self, uri, headers, cancellable,
    callback, data);

  if (headers)
    g_hash_table_unref (headers);
}

inline static void
eam_wc_assure_filename (EamWc *self, const gchar *uri_path)
{
  EamWcPrivate *priv = eam_wc_get_instance_private (self);

  if (priv->filename)
    return;

  priv->filename = g_path_get_basename (uri_path);
  if (!g_strcmp0 (priv->filename, G_DIR_SEPARATOR_S) ||
      !g_strcmp0 (priv->filename, ".")) {
    g_free (priv->filename);
    priv->filename = g_strdup ("./output.tmp");
  }
}

/**
 * eam_wc_request_with_headers_hash_async:
 * @self: a #EamWc instance
 * @uri: The URI of the resource to request
 * @headers: (allow-none) (element-type utf8 utf8): a set of additional HTTP
 * headers for this request or %NULL to ignore
 * @cancellable: (allow-none): a #GCancellable instance or %NULL to ignore
 * @callback: The callback when the result is ready
 * @data: User data set for the @callback
 *
 * Request the fetching of a web resource given the @uri. This request is
 * asynchronous, thus the result will be returned within the @callback.
 */
void
eam_wc_request_with_headers_hash_async (EamWc *self, const char *uri,
  GHashTable *headers, GCancellable *cancellable, GAsyncReadyCallback callback,
  gpointer data)
{
  g_return_if_fail (EAM_IS_WC (self));
  g_return_if_fail (uri);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  EamWcPrivate *priv = eam_wc_get_instance_private (self);
  GTask *task = g_task_new (self, cancellable, callback, data);

  SoupURI *suri = soup_uri_new (uri);
  eam_wc_assure_filename (self, soup_uri_get_path (suri));

  GError *error = NULL;
  SoupRequest *request = soup_session_request_uri (priv->session, suri, &error);
  soup_uri_free (suri);

  if (!request) {
    soup_uri_free (suri);
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  if (headers) {
    SoupMessage *message = soup_request_http_get_message (SOUP_REQUEST_HTTP (request));
    if (message) {
      GHashTableIter iter;
      const gchar *key, *value;

      g_hash_table_iter_init (&iter, headers);
      while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *)  &value)) {
        soup_message_headers_append (message->request_headers, key, value);
      }
      g_object_unref (message);
    }
  }

  struct task_clos *clos = g_slice_new0 (struct task_clos);
  g_task_set_task_data (task, clos, (GDestroyNotify) task_closure_free);

  soup_request_send_async (request, cancellable, request_cb, task);
}


/**
 * eam_wc_request_finish:
 * @self: a #EamWc instance
 * @result: The result of the request
 * @content: The contents of the resource
 * @length: (allow-none): The length of the contents or %NULL if it is not
 * needed
 * @error: return location for a #GError, or %NULL
 *
 * Finishes an asynchronous load of the file's contents.
 * The contents are placed in contents, and length is set to the size of the
 * contents string.
 *
 * The content address will be invalidated at the next request. So if you
 * want to keep it, please copy it into another address.
 *
 * Returns: %TRUE if the request was successfull. If %FALSE an error occurred.
 */
gboolean
eam_wc_request_finish (EamWc *self, GAsyncResult *result, gchar **content,
  gsize *length, GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  eam_wc_reset (self);
  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * eam_wc_set_log_level:
 * @self: a #EamWc instance
 * @level: the libsoup log level to set [0,3]
 *
 * Setting the log level the logger feature is added into
 * the libsoup session.
 */
void
eam_wc_set_log_level (EamWc *self, guint level)
{
  g_return_if_fail (level <= 3);
  g_return_if_fail (EAM_IS_WC (self));

  EamWcPrivate *priv = eam_wc_get_instance_private (self);
  if (priv->level == level)
    return;

  soup_session_remove_feature_by_type (priv->session, SOUP_TYPE_LOGGER);

  SoupLogger *logger = soup_logger_new ((SoupLoggerLogLevel) level, -1);
  soup_session_add_feature (priv->session, SOUP_SESSION_FEATURE (logger));
  g_object_unref (logger);

  priv->level = (SoupLoggerLogLevel) level;
}

EamWc *
eam_wc_new (void)
{
  return g_object_new (EAM_TYPE_WC, NULL);
}
