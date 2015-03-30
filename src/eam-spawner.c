/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include <glib/gi18n.h>

#include "eam-error.h"
#include "eam-spawner.h"
#include "eam-log.h"

typedef struct _EamSpawnerPrivate EamSpawnerPrivate;

struct _EamSpawnerPrivate
{
  GFile *dir;
  GStrv params;
  GHashTable *environment;
};

typedef struct _TaskData TaskData;

struct _TaskData
{
  GSubprocessLauncher *launcher;
  GList *file_names;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamSpawner, eam_spawner, G_TYPE_OBJECT)

enum {
  PROP_DIR = 1,
  PROP_PARAMS,
  PROP_ENV
};

static void
eam_spawner_finalize (GObject *obj)
{
  EamSpawnerPrivate *priv = eam_spawner_get_instance_private (EAM_SPAWNER (obj));

  g_object_unref (priv->dir);

  g_strfreev (priv->params);

  if (priv->environment != NULL)
    g_hash_table_unref (priv->environment);

  G_OBJECT_CLASS (eam_spawner_parent_class)->finalize (obj);
}

static void
eam_spawner_set_property (GObject *obj, guint propid, const GValue *value,
  GParamSpec *pspec)
{
  EamSpawnerPrivate *priv = eam_spawner_get_instance_private (EAM_SPAWNER (obj));

  switch (propid) {
  case PROP_DIR:
    priv->dir = g_value_dup_object (value);
    break;
  case PROP_PARAMS:
    priv->params = g_strdupv (g_value_get_boxed (value));
    break;
  case PROP_ENV:
    if (g_value_get_boxed (value) != NULL)
      priv->environment = g_hash_table_ref (g_value_get_boxed (value));
    break;
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
  case PROP_DIR:
    g_value_set_object (value, priv->dir);
    break;
  case PROP_PARAMS:
    g_value_set_boxed (value, priv->params);
    break;
  case PROP_ENV:
    g_value_set_boxed (value, priv->environment);
    break;
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
    g_param_spec_object ("dir", "Directory", "Directory GFile", G_TYPE_FILE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

   /**
   * EamSpawner::params:
   *
   * The parameters to be passed to the scripts on the "dir" directory
   */
  g_object_class_install_property (object_class, PROP_PARAMS,
    g_param_spec_boxed ("params", _("Parameters"), _("List of parameters to be passed to the script"), G_TYPE_STRV,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_ENV,
    g_param_spec_boxed ("env", "Environment", "Environment for the scripts", G_TYPE_HASH_TABLE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
eam_spawner_init (EamSpawner *self)
{
}

/**
 * eam_spawner_new:
 * @path: The path of the directory with the scripts to execute.
 *
 * Create a new instance of #EamSpawner with the default appdir.
 */
EamSpawner *
eam_spawner_new (const gchar *path, GHashTable *env, const gchar * const *params)
{
  GFile *dir = g_file_new_for_path (path);

  EamSpawner *ret = g_object_new (EAM_TYPE_SPAWNER,
                                  "dir", dir,
                                  "env", env,
                                  "params", params,
                                  NULL);
  g_object_unref (dir);

  return ret;
}

static void subprocess_run_async (GTask *task);

typedef struct {
  GSubprocess *process;
  GTask *task;
  char *script_name;
} ScriptData;

static ScriptData *
script_data_new (GTask *task, const char *file_name, GSubprocess *process)
{
  ScriptData *data = g_slice_new (ScriptData);

  data->process = process;
  data->task = task;
  data->script_name = g_path_get_basename (file_name);

  return data;
}

static void
script_data_free (ScriptData *data)
{
  g_free (data->script_name);
  g_slice_free (ScriptData, data);
}

static void
script_data_maybe_log_stdout (ScriptData *data)
{
  GOutputStream *buffer = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
  GError *error = NULL;

  g_output_stream_splice (buffer,
			  g_subprocess_get_stdout_pipe (data->process),
                          0, NULL, &error);

 if (error != NULL) {
    eam_log_error_message ("Unable to splice the stdout pipe: %s", error->message);
    g_error_free (error);
  } else {
    g_output_stream_write (buffer, "\0", 1, NULL, NULL);
    g_output_stream_close (buffer, NULL, NULL);
    char *str = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (buffer));
    if (strlen (str) > 0) {
      eam_log_info_message ("Script '%s' wrote to stdout", data->script_name);
      eam_log_info_message ("%s", str);
    }
    g_free (str);
  }

  g_object_unref (buffer);
}

static void
script_data_maybe_log_stderr (ScriptData *data)
{
  GOutputStream *buffer = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
  GError *error = NULL;

  g_output_stream_splice (buffer,
			  g_subprocess_get_stderr_pipe (data->process),
                          0, NULL, &error);

  if (error != NULL) {
    eam_log_error_message ("Unable to splice the stderr pipe: %s", error->message);
    g_error_free (error);
  } else {
    g_output_stream_write (buffer, "\0", 1, NULL, NULL);
    g_output_stream_close (buffer, NULL, NULL);
    char *str = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (buffer));
    if (strlen (str) > 0) {
      eam_log_error_message ("Script '%s' wrote to stderr", data->script_name);
      eam_log_error_message ("%s", str);
    }
    g_free (str);
  }

  g_object_unref (buffer);
}

static void
subprocess_cb (GObject *source, GAsyncResult *res, gpointer data_)
{
  ScriptData *data = data_;
  GTask *task = data->task;
  const char *script_name = data->script_name;
  GError *error = NULL;
  GSubprocess *process = G_SUBPROCESS (source);

  script_data_maybe_log_stdout (data);
  script_data_maybe_log_stderr (data);

  g_subprocess_wait_finish (process, res, &error);
  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  if (!g_subprocess_get_successful (process)) {
    g_task_return_new_error (task, EAM_ERROR,
      EAM_ERROR_FAILED,
      _("Script \"%s\" exited with error code %d"),
      script_name, g_subprocess_get_exit_status (process));

    goto bail;
  }

  script_data_free (data);

  subprocess_run_async (task);

  return;

bail:
  script_data_free (data);
  g_object_unref (task);
}

static void
subprocess_run_async (GTask *task)
{
  TaskData *task_data = g_task_get_task_data (task);
  GError *error = NULL;

  if (!task_data->file_names) {
    g_task_return_boolean (task, TRUE);
    goto bail;
  }

  gchar *file_name = task_data->file_names->data;
  task_data->file_names = g_list_delete_link (task_data->file_names, task_data->file_names);

  EamSpawner *self = g_task_get_source_object (task);
  EamSpawnerPrivate *priv = eam_spawner_get_instance_private (self);
  GStrv argv = g_new0 (gchar*, priv->params ? g_strv_length (priv->params) + 2 : 2);
  argv[0] = file_name;

  int i = 0;
  while (priv->params && priv->params[i]) {
    argv[i+1] = g_strdup (priv->params[i]);
    ++i;
  }

  GSubprocess *process = g_subprocess_launcher_spawnv (task_data->launcher, (const char **) argv, &error);

  if (error) {
    g_task_return_error (task, error);
    goto bail;
  }

  ScriptData *data = script_data_new (task, file_name, process);

  /* Freed here as 'file_name' is used in the previous call */
  g_strfreev (argv);

  GCancellable *cancellable = g_task_get_cancellable (task);
  g_subprocess_wait_async (process, cancellable, subprocess_cb, data);
  g_object_unref (process);

  return;

bail:
  g_object_unref (task);
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
    g_object_unref (task);
    return;
  }

  if (g_task_return_error_if_cancelled (task)) {
    g_object_unref (task);
    return;
  }

  TaskData *task_data = g_task_get_task_data (task);
  task_data->file_names = g_list_sort (task_data->file_names, (GCompareFunc) g_strcmp0);

  subprocess_run_async (task);
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

  if (!g_content_type_is_a (g_file_info_get_content_type (info),
      "application/x-shellscript"))
    goto next;

  GFile *child = g_file_enumerator_get_child (fenum, info);
  gchar *file_name = g_file_get_path (child);
  g_object_unref (child);

  TaskData *task_data = g_task_get_task_data (task);
  task_data->file_names = g_list_prepend (task_data->file_names, file_name);

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
task_data_free (gpointer data)
{
  TaskData *task_data = data;

  g_object_unref (task_data->launcher);
  g_list_free_full (task_data->file_names, g_free);
  g_slice_free (TaskData, task_data);
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

  EamSpawner *self = g_task_get_source_object (task);
  EamSpawnerPrivate *priv = eam_spawner_get_instance_private (self);

  TaskData *task_data = g_slice_new0 (TaskData);
  g_task_set_task_data (task, task_data, task_data_free);

  GSubprocessFlags flags = G_SUBPROCESS_FLAGS_STDOUT_PIPE
                         | G_SUBPROCESS_FLAGS_STDERR_PIPE;

  task_data->launcher = g_subprocess_launcher_new (flags);

  if (priv->environment != NULL) {
    GHashTableIter iter;
    g_hash_table_iter_init (&iter, priv->environment);

    gpointer var_name, var_value;
    while (g_hash_table_iter_next (&iter, &var_name, &var_value))
      g_subprocess_launcher_setenv (task_data->launcher, var_name, var_value, FALSE);
  }

  GCancellable *cancellable = g_task_get_cancellable (task);
  g_file_enumerator_next_files_async (fenum, 1, G_PRIORITY_DEFAULT,
     cancellable, got_file, data);
  g_object_unref (fenum);

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
