/* Copyright 2014 Endless Mobile, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "eam-spawner.h"

typedef struct _EamSpawnerPrivate EamSpawnerPrivate;

struct _EamSpawnerPrivate
{
  GFile *dir;
  char *scriptname;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamSpawner, eam_spawner, G_TYPE_OBJECT)

G_DEFINE_QUARK (eam-spawner-error-quark, eam_spawner_error)

enum {
  PROP_DIR = 1,
};

static void
eam_spawner_finalize (GObject *obj)
{
  EamSpawnerPrivate *priv = eam_spawner_get_instance_private (EAM_SPAWNER (obj));

  g_object_unref (priv->dir);
  g_free (priv->scriptname);

  G_OBJECT_CLASS (eam_spawner_parent_class)->finalize (obj);
}

static void
eam_spawner_set_property (GObject *obj, guint propid, const GValue *value,
  GParamSpec *pspec)
{
  EamSpawnerPrivate *priv = eam_spawner_get_instance_private (EAM_SPAWNER (obj));

  switch (propid) {
  case PROP_DIR: {
    const gchar *path = g_value_get_string (value);
    priv->dir = g_file_new_for_path (path);
    break;
  }
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, propid, pspec);
    break;
  }
}

static void
eam_spawner_get_property (GObject *obj, guint propid, GValue *value,
  GParamSpec *pspec)
{
  EamSpawnerPrivate *priv = eam_spawner_get_instance_private (EAM_SPAWNER (obj));

  switch (propid) {
  case PROP_DIR: {
    gchar *path = g_file_get_path (priv->dir);
    g_value_take_string (value, path);
    break;
  }
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, propid, pspec);
    break;
  }
}

static void
eam_spawner_class_init (EamSpawnerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eam_spawner_finalize;
  object_class->set_property = eam_spawner_set_property;
  object_class->get_property = eam_spawner_get_property;

  /**
   * EamSpawner::dir:
   *
   * The directory where the script to run are
   */
  g_object_class_install_property (object_class, PROP_DIR,
    g_param_spec_string ("dir", "Directory", "Directory Path", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |  G_PARAM_STATIC_STRINGS));
}

static void
eam_spawner_init (EamSpawner *self)
{
}

/**
 * eam_spawner_new:
 *
 * Create a new instance of #EamSpawner with the default appdir.
 */
EamSpawner *
eam_spawner_new (const gchar *dir)
{
  return g_object_new (EAM_TYPE_SPAWNER, "dir", dir, NULL);
}

static void
enum_close_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  GTask *task = data;
  GError *error = NULL;
  GFileEnumerator *fenum = G_FILE_ENUMERATOR (source);

  g_file_enumerator_close_finish (fenum, res, &error);
  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  if (g_task_return_error_if_cancelled (task))
    goto bail;

  g_task_return_boolean (task, TRUE);

bail:
  g_object_unref (task);
}

static void got_file (GObject *source, GAsyncResult *res, gpointer data);

static void
subprocess_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  GTask *task = data;
  GError *error = NULL;
  GSubprocess *process = G_SUBPROCESS (source);

  g_subprocess_wait_finish (process, res, &error);
  if (error) {
    g_task_return_error (task, error);
    g_object_unref (source);
    g_object_unref (task);
    return;
  }

  if (!g_subprocess_get_successful (process)) {
    EamSpawner *self = g_task_get_source_object (task);
    EamSpawnerPrivate *priv = eam_spawner_get_instance_private (self);

    g_task_return_new_error (task, EAM_SPAWNER_ERROR,
      EAM_SPAWNER_ERROR_SCRIPT_FAILED, "script \"%s\" exited with error code %d",
      priv->scriptname, g_subprocess_get_exit_status (process));
    g_object_unref (source);
    g_object_unref (task);
    return;
  }

  /* Get more files */
  GFileEnumerator *fenum = g_task_get_task_data (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_file_enumerator_next_files_async (fenum, 1, G_PRIORITY_DEFAULT, cancellable,
    got_file, task);

  g_object_unref (source);
}

static void
got_file (GObject *source, GAsyncResult *res, gpointer data)
{
  GTask *task = data;
  GError *error = NULL;
  GFileEnumerator *fenum = G_FILE_ENUMERATOR (source);

  GList *infos = g_file_enumerator_next_files_finish (fenum, res, &error);
  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  if (g_task_return_error_if_cancelled (task))
    goto bail;

  GCancellable *cancellable = g_task_get_cancellable (task);

  if (!infos) {
    g_file_enumerator_close_async (fenum, G_PRIORITY_DEFAULT, cancellable,
      enum_close_cb, task);
    return;
  }

  GFileInfo *info = infos->data;
  if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR)
    goto next;

  if (!g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE))
    goto next;

  if (g_strcmp0 ("application/x-shellscript", g_file_info_get_content_type (info)) != 0)
    goto next;

  EamSpawner *self = g_task_get_source_object (task);
  EamSpawnerPrivate *priv = eam_spawner_get_instance_private (self);

  {
    g_free (priv->scriptname);
    priv->scriptname = g_strdup (g_file_info_get_name (info));
  }

  gchar *dir = g_file_get_path (priv->dir);
  gchar *fname = g_build_filename (dir, priv->scriptname, NULL);

  /* @TODO: connect stdout & stderr to a logging subsystem */
  GSubprocess *process = g_subprocess_new (G_SUBPROCESS_FLAGS_NONE, &error,
     fname, NULL);

  g_free (dir);
  g_free (fname);

  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  g_list_free_full (infos, g_object_unref);
  g_subprocess_wait_async (process, cancellable, subprocess_cb, task);

  return;

next:
  g_list_free_full (infos, g_object_unref);

  /* Not last, get more files */
  g_file_enumerator_next_files_async (fenum, 1, G_PRIORITY_DEFAULT, cancellable,
    got_file, task);

  return;

bail:
  g_list_free_full (infos, g_object_unref);
  g_object_unref (task);
}

static void
got_enum (GObject *source, GAsyncResult *res, gpointer data)
{
  GTask *task = data;
  GError *error = NULL;

  GFileEnumerator *fenum = g_file_enumerate_children_finish (G_FILE (source),
    res, &error);

  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  if (g_task_return_error_if_cancelled (task))
    goto bail;

  g_task_set_task_data (task, fenum, g_object_unref);

  GCancellable *cancellable = g_task_get_cancellable (task);
  g_file_enumerator_next_files_async (fenum, 1, G_PRIORITY_DEFAULT,
     cancellable, got_file, data);

  return;

bail:
  g_object_unref (task);
}

void
eam_spawner_run_async (EamSpawner *self, GCancellable *cancellable,
  GAsyncReadyCallback callback, gpointer data)
{
  g_return_if_fail (EAM_IS_SPAWNER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  EamSpawnerPrivate *priv = eam_spawner_get_instance_private (self);
  GTask *task = g_task_new (self, cancellable, callback, data);

  /* walk over the directory, executing  */
  g_file_enumerate_children_async (priv->dir,
    G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE "," G_FILE_ATTRIBUTE_STANDARD_NAME "," \
    G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, G_PRIORITY_DEFAULT, cancellable,
    got_enum, task);
}

gboolean
eam_spawner_run_finish (EamSpawner *self, GAsyncResult *result, GError **error)
{
  g_return_val_if_fail (EAM_IS_SPAWNER (self), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
