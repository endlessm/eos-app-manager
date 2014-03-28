/* Copyright 2014 Endless Mobile, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>

#include "eam-wc-file.h"

typedef struct _EamWcFilePrivate EamWcFilePrivate;

struct _EamWcFilePrivate
{
  GFile *file;
  GOutputStream *strm;
  gssize size;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamWcFile, eam_wc_file, G_TYPE_OBJECT)

enum {
  PROP_SIZE = 1,
};

static void
eam_wc_file_set_property (GObject *obj, guint propid, const GValue *value,
  GParamSpec *pspec)
{
  EamWcFilePrivate *priv = eam_wc_file_get_instance_private (EAM_WC_FILE (obj));

  switch (propid) {
  case PROP_SIZE:
    priv->size = g_value_get_uint64 (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, propid, pspec);
  }
}

static void
eam_wc_file_get_property (GObject *obj, guint propid, GValue *value,
  GParamSpec *pspec)
{
  EamWcFilePrivate *priv = eam_wc_file_get_instance_private (EAM_WC_FILE (obj));

  switch (propid) {
  case PROP_SIZE:
    g_value_set_uint64 (value, priv->size);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, propid, pspec);
  }
}

static void
eam_wc_file_reset (EamWcFile *self)
{
  EamWcFilePrivate *priv = eam_wc_file_get_instance_private (self);

  g_clear_object (&priv->strm);
  g_clear_object (&priv->file);
}

static void
eam_wc_file_finalize (GObject *obj)
{
  eam_wc_file_reset (EAM_WC_FILE (obj));
  G_OBJECT_CLASS (eam_wc_file_parent_class)->finalize (obj);
}

static void
eam_wc_file_class_init (EamWcFileClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = eam_wc_file_finalize;
  object_class->set_property = eam_wc_file_set_property;
  object_class->get_property = eam_wc_file_get_property;

  /**
   * EamWc::size:
   *
   * The the file size.
   * 0 if it's unknown.
   */
  g_object_class_install_property (object_class, PROP_SIZE,
    g_param_spec_uint64 ("size", "File size", "",
      0, G_MAXUINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
eam_wc_file_init (EamWcFile *self)
{
}

static void
replace_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GTask *task = data;
  GError *error = NULL;
  GFileOutputStream *strm = g_file_replace_finish (G_FILE (source), result, &error);
  if (!strm) {
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  EamWcFile *file = g_task_get_source_object (task);
  EamWcFilePrivate *priv = eam_wc_file_get_instance_private (file);
  g_assert (!priv->strm);

  priv->strm = G_OUTPUT_STREAM (strm);

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

static void
query_fs_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GFile *parent = G_FILE (source);
  GTask *task = data;
  GError *error = NULL;
  GFileInfo *info = g_file_query_filesystem_info_finish (parent, result, &error);

  g_object_unref (parent);

  if (error) {
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  guint64 free = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
  g_object_unref (info);

  if (g_task_return_error_if_cancelled (task)) {
    g_object_unref (task);
    return;
  }

  EamWcFile *self = g_task_get_source_object (task);
  EamWcFilePrivate *priv = eam_wc_file_get_instance_private (self);

  /* let's be cautious: request twice the size or 20K, please */
  gsize reqsiz = (priv->size) ? priv->size * 2 : 20 * 1024;
  g_debug ("Requesting %li bytes from %li bytes of free space", reqsiz, free);

  if (free < reqsiz) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
      _("Not enough free space in device"));
    g_object_unref (task);
    return;
  }

  GCancellable *cancellable = g_task_get_cancellable (task);
  g_file_replace_async (priv->file, NULL, FALSE,
    G_FILE_CREATE_PRIVATE | G_FILE_CREATE_REPLACE_DESTINATION,
    G_PRIORITY_DEFAULT, cancellable, replace_cb, task);
}

void
eam_wc_file_open_async (EamWcFile *self, const char *path,
  GCancellable *cancellable, GAsyncReadyCallback callback, gpointer data)
{
  g_return_if_fail (EAM_IS_WC_FILE (self));
  g_return_if_fail (path);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  EamWcFilePrivate *priv = eam_wc_file_get_instance_private (self);
  GTask *task = g_task_new (self, cancellable, callback, data);

  priv->file = g_file_new_for_path (path);

  /* 1.create the parent directory */
  GFile *parent = g_file_get_parent (priv->file);
  if (parent) {
    GError *error = NULL;
    g_file_make_directory_with_parents (parent, cancellable, &error);
    if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS)) {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }
    if (error)
      g_error_free (error);
  }

  /* 1. check if there's "enough" space */
  g_file_query_filesystem_info_async (parent,
    G_FILE_ATTRIBUTE_FILESYSTEM_FREE, G_PRIORITY_DEFAULT, cancellable,
    query_fs_cb, task);
}

gboolean
eam_wc_file_open_finish (EamWcFile *self, GAsyncResult *result, GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
splice_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GTask *task = data;

  GError *error = NULL;
  gssize size = g_output_stream_splice_finish (G_OUTPUT_STREAM (source), result, &error);
  if (error) {
    g_task_return_error (task, error);
    goto done;
  }

  g_task_return_int (task, size);

done:
  {
    EamWcFile *self = g_task_get_source_object (task);
    eam_wc_file_reset (self); /* let's close everything */

    g_object_unref (task);
  }
}

void
eam_wc_file_splice_async (EamWcFile *self, GInputStream *source,
  GCancellable *cancellable, GAsyncReadyCallback callback, gpointer data)
{
  g_return_if_fail (EAM_IS_WC_FILE (self));
  g_return_if_fail (G_IS_INPUT_STREAM (source));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  GTask *task = g_task_new (self, cancellable, callback, data);
  EamWcFilePrivate *priv = eam_wc_file_get_instance_private (self);

  g_output_stream_splice_async (priv->strm, source,
    G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
    G_PRIORITY_DEFAULT, cancellable, splice_cb, task);
}

gssize
eam_wc_file_splice_finish (EamWcFile *self, GAsyncResult *result, GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  return g_task_propagate_int (G_TASK (result), error);
}

EamWcFile *
eam_wc_file_new ()
{
  return g_object_new (EAM_TYPE_WC_FILE, NULL);
}
