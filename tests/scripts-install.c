/* Copyright 2014 Endless Mobile, Inc. */

#include <glib/gstdio.h>
#include <eam-spawner.h>

const gchar *script_args[3];

static void
run_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  GError *error = NULL;
  eam_spawner_run_finish (EAM_SPAWNER (source), res, &error);
  if (error) {
    g_print ("Error: %s\n", error->message);
    g_clear_error (&error);
    g_test_fail ();
  }
  g_main_loop_quit (data);
}

static void
test_scripts_install (void)
{
  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, "../scripts/install", NULL);

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);

  eam_spawner_run_async (spawner, NULL, run_cb, loop);
  g_object_unref (spawner);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);
}

static void
test_scripts_install_rollback (void)
{
  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, "../scripts/install_rollback", NULL);

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);

  eam_spawner_run_async (spawner, NULL, run_cb, loop);
  g_object_unref (spawner);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);
}

static void
directories_initialization (void)
{
  /* Replicate the OS structure in a temporary directory */
  const gint mode = 0700;
  g_mkdir ("/tmp/endless", mode);
  g_mkdir ("/tmp/endless/share", mode);
  g_mkdir ("/tmp/endless/share/applications", mode);
  g_mkdir ("/tmp/endless/share/dbus-1", mode);
  g_mkdir ("/tmp/endless/share/dbus-1/services", mode);
  g_mkdir ("/tmp/endless/share/glib-2.0", mode);
  g_mkdir ("/tmp/endless/share/glib-2.0/schemas", mode);
  g_mkdir ("/tmp/endless/share/icons", mode);
  g_mkdir ("/tmp/endless/share/icons/EndlessOS", mode);

  /* Copy the bundle and sha256 files to a temporary directory */
  gchar *app_filename = g_test_build_filename (G_TEST_DIST, "data/test-app.tar.gz", NULL);
  gchar *sha256_filename = g_test_build_filename (G_TEST_DIST, "data/test-app.sha256", NULL);
  GFile *app_file = g_file_new_for_path (app_filename);
  GFile *sha256_file = g_file_new_for_path (sha256_filename);
  GFile *app_file_tmp = g_file_new_for_path ("/tmp/test-app.tar.gz");
  GFile *sha256_file_tmp = g_file_new_for_path ("/tmp/test-app.sha256");
  GError *error = NULL;

  if ((!g_file_copy (app_file, app_file_tmp, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error)) ||
      (!g_file_copy (sha256_file, sha256_file_tmp, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error))) {
    g_print ("Failure in the test initialization: %s\n", error->message);
    g_error_free (error);
    g_assert_not_reached ();
  }

  g_free (app_filename);
  g_free (sha256_filename);
  g_object_unref (app_file);
  g_object_unref (sha256_file);
  g_object_unref (app_file_tmp);
  g_object_unref (sha256_file_tmp);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  /* Setup a temporary OS directory structure */
  directories_initialization ();

  /* Set environment variables */
  g_setenv ("PREFIX", "/tmp/endless", TRUE);
  g_setenv ("TMP", "/tmp", TRUE);

  /* Initialize script arguments */
  script_args[0] = "test-app";
  script_args[1] = "/tmp/test-app.tar.gz";
  script_args[2] = NULL;

  g_test_add_func ("/scripts/install", test_scripts_install);
  g_test_add_func ("/scripts/install-rollback", test_scripts_install_rollback);

  return g_test_run ();
}
