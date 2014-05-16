/* Copyright 2014 Endless Mobile, Inc. */

#include <glib/gstdio.h>
#include <eam-spawner.h>

#include "eam-fs-sanity.h"

#define EAM_TEST_DIR "/tmp/eam-test"
#define EAM_PREFIX EAM_TEST_DIR "/endless"
#define EAM_TMP EAM_TEST_DIR "/tmp"

const gchar *script_args[3];

static void
setup_filesystem (void)
{
  /* Replicate the OS structure in a temporary directory */
  const gint mode = 0700;
  g_mkdir (EAM_TEST_DIR, mode);
  g_mkdir (EAM_TMP, mode);
  g_mkdir (EAM_PREFIX, mode);
  g_mkdir (EAM_PREFIX "/share", mode);
  g_mkdir (EAM_PREFIX "/share/applications", mode);
  g_mkdir (EAM_PREFIX "/share/dbus-1", mode);
  g_mkdir (EAM_PREFIX "/share/dbus-1/services", mode);
  g_mkdir (EAM_PREFIX "/share/glib-2.0", mode);
  g_mkdir (EAM_PREFIX "/share/glib-2.0/schemas", mode);
  g_mkdir (EAM_PREFIX "/share/icons", mode);
  g_mkdir (EAM_PREFIX "/share/icons/EndlessOS", mode);
}

static void
clear_filesystem (void)
{
  if (!g_file_test (EAM_TEST_DIR, G_FILE_TEST_EXISTS))
    return;

  if (!eam_fs_sanity_delete (EAM_TEST_DIR))
    g_print ("Failure when cleaning the test files");
}

static void
setup_downloaded_files (void)
{
  /* Copy the bundle and sha256 files to a temporary directory */
  gchar *app_filename = g_test_build_filename (G_TEST_DIST, "data/test-app.tar.gz", NULL);
  gchar *sha256_filename = g_test_build_filename (G_TEST_DIST, "data/test-app.sha256", NULL);
  GFile *app_file = g_file_new_for_path (app_filename);
  GFile *sha256_file = g_file_new_for_path (sha256_filename);
  GFile *app_file_tmp = g_file_new_for_path (EAM_TMP "/test-app.tar.gz");
  GFile *sha256_file_tmp = g_file_new_for_path (EAM_TMP "/test-app.sha256");
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
  setup_filesystem ();
  setup_downloaded_files ();

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, "../scripts/install", NULL);

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);

  eam_spawner_run_async (spawner, NULL, run_cb, loop);
  g_object_unref (spawner);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);

  clear_filesystem ();
}

static void
install_rollback_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  GError *error = NULL;
  eam_spawner_run_finish (EAM_SPAWNER (source), res, &error);
  if (error) {
    g_print ("Error: %s\n", error->message);
    g_clear_error (&error);

    g_test_fail ();
    g_main_loop_quit (data);

    return;
  }

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, "../scripts/install_rollback", NULL);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);

  eam_spawner_run_async (spawner, NULL, run_cb, data);
  g_object_unref (spawner);
}

static void
test_scripts_install_rollback (void)
{
  setup_filesystem ();
  setup_downloaded_files ();

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, "../scripts/install", NULL);

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);
  eam_spawner_run_async (spawner, NULL, install_rollback_cb, loop);
  g_object_unref (spawner);

  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  clear_filesystem ();
}


static void
install_uninstall_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  GError *error = NULL;
  eam_spawner_run_finish (EAM_SPAWNER (source), res, &error);
  if (error) {
    g_print ("Error: %s\n", error->message);
    g_clear_error (&error);

    g_test_fail ();
    g_main_loop_quit (data);

    return;
  }

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, "../scripts/uninstall", NULL);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);

  eam_spawner_run_async (spawner, NULL, run_cb, data);
  g_object_unref (spawner);
}

static void
test_scripts_install_uninstall (void)
{
  setup_filesystem ();
  setup_downloaded_files ();

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, "../scripts/install", NULL);

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);
  eam_spawner_run_async (spawner, NULL, install_uninstall_cb, loop);
  g_object_unref (spawner);

  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  clear_filesystem ();
}

static void
install_update_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  GError *error = NULL;
  eam_spawner_run_finish (EAM_SPAWNER (source), res, &error);
  if (error) {
    g_print ("Error: %s\n", error->message);
    g_clear_error (&error);

    g_test_fail ();
    g_main_loop_quit (data);

    return;
  }

  setup_downloaded_files ();

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, "../scripts/update/full_update", NULL);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);

  eam_spawner_run_async (spawner, NULL, run_cb, data);
  g_object_unref (spawner);
}

static void
test_scripts_install_update (void)
{
  setup_filesystem ();
  setup_downloaded_files ();

  /* Pre-install the application */
  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, "../scripts/install", NULL);

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);
  eam_spawner_run_async (spawner, NULL, install_update_cb, loop);
  g_object_unref (spawner);

  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  clear_filesystem ();
}

static void
update_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  GError *error = NULL;
  eam_spawner_run_finish (EAM_SPAWNER (source), res, &error);
  if (error) {
    g_print ("Error: %s\n", error->message);
    g_clear_error (&error);

    g_test_fail ();
    g_main_loop_quit (data);

    return;
  }

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, "../scripts/update/rollback", NULL);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);

  eam_spawner_run_async (spawner, NULL, run_cb, data);
  g_object_unref (spawner);
}

static void
install_update_rollback_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  GError *error = NULL;
  eam_spawner_run_finish (EAM_SPAWNER (source), res, &error);
  if (error) {
    g_print ("Error: %s\n", error->message);
    g_clear_error (&error);

    g_test_fail ();
    g_main_loop_quit (data);

    return;
  }

  setup_downloaded_files ();

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, "../scripts/update/full_update", NULL);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);

  eam_spawner_run_async (spawner, NULL, update_cb, data);
  g_object_unref (spawner);
}

static void
test_scripts_install_update_rollback (void)
{
  setup_filesystem ();
  setup_downloaded_files ();

  /* Pre-install the application */
  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, "../scripts/install", NULL);

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);
  eam_spawner_run_async (spawner, NULL, install_update_rollback_cb, loop);
  g_object_unref (spawner);

  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  clear_filesystem ();
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  /* Set environment variables */
  g_setenv ("PREFIX", EAM_PREFIX, TRUE);
  g_setenv ("TMP", EAM_TMP, TRUE);

  /* Initialize script arguments */
  script_args[0] = "test-app";
  script_args[1] = EAM_TMP "/test-app.tar.gz";
  script_args[2] = NULL;

  g_test_add_func ("/scripts/install", test_scripts_install);
  g_test_add_func ("/scripts/install-rollback", test_scripts_install_rollback);
  g_test_add_func ("/scripts/install-uninstall", test_scripts_install_uninstall);
  g_test_add_func ("/scripts/install-update", test_scripts_install_update);
  g_test_add_func ("/scripts/install-update-rollback", test_scripts_install_update_rollback);
  return g_test_run ();
}
