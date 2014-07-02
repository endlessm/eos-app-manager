/* Copyright 2014 Endless Mobile, Inc. */

#include <glib/gstdio.h>
#include <eam-spawner.h>

#include "eam-fs-sanity.h"

#define EAM_TEST_DIR "/tmp/eam-test"
#define EAM_PREFIX EAM_TEST_DIR "/endless"
#define EAM_TMP EAM_TEST_DIR "/tmp"
#define EAM_GPGKEYRING EAM_TEST_DIR "/keyring.gpg"

#define SCRIPTDIR_INSTALL "../scripts/install/run"
#define SCRIPTDIR_INSTALL_ROLLBACK "../scripts/install/rollback"
#define SCRIPTDIR_UNINSTALL "../scripts/uninstall"
#define SCRIPTDIR_FULL_UPDATE "../scripts/update/full"
#define SCRIPTDIR_FULL_UPDATE_ROLLBACK "../scripts/update/rollback"

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
  g_mkdir (EAM_PREFIX "/share/ekn", mode);
  g_mkdir (EAM_PREFIX "/share/ekn/data", mode);
  g_mkdir (EAM_PREFIX "/share/ekn/manifest", mode);
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

static gboolean
copy_file (const gchar *fn, GError **error)
{
  gchar *filename = g_test_build_filename (G_TEST_DIST, "data", fn, NULL);
  GFile *file = g_file_new_for_path (filename);
  g_free (filename);

  gchar *filename_tmp = g_build_filename (EAM_TMP, fn, NULL);
  GFile *file_tmp = g_file_new_for_path (filename_tmp);
  g_free (filename_tmp);

  gboolean ret = g_file_copy (file, file_tmp, G_FILE_COPY_OVERWRITE, NULL, NULL,
    NULL, error);

  g_object_unref (file);
  g_object_unref (file_tmp);

  return ret;
}

static void
setup_downloaded_files (void)
{
  /* Copy the bundle and sha256 files to a temporary directory */
  const gchar *files[] = { "test-app.tar.gz", "test-app.sha256",
    "test-app.asc" };

  int i;
  GError *error = NULL;
  for (i = 0; i < G_N_ELEMENTS (files); i++) {
    if (!copy_file (files[i], &error)) {
      g_print ("Failure in the test initialization: %s\n", error->message);
      g_error_free (error);
      g_assert_not_reached ();
    }
  }
}

static void
setup_gpg (void)
{
  gchar *key = g_test_build_filename (G_TEST_DIST, "data", "dummy.pub", NULL);
  GSubprocess *gpg = g_subprocess_new (G_SUBPROCESS_FLAGS_NONE, NULL, "gpg",
    "--keyring", EAM_GPGKEYRING, "--no-default-keyring", "--import", key, NULL);
  g_free (key);

  g_assert_nonnull (gpg);

  g_assert_true (g_subprocess_wait_check (gpg, NULL, NULL));
  g_object_unref (gpg);
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
  setup_gpg ();

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, SCRIPTDIR_INSTALL, NULL);

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

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, SCRIPTDIR_INSTALL_ROLLBACK, NULL);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);

  eam_spawner_run_async (spawner, NULL, run_cb, data);
  g_object_unref (spawner);
}

static void
test_scripts_install_rollback (void)
{
  setup_filesystem ();
  setup_downloaded_files ();
  setup_gpg ();

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, SCRIPTDIR_INSTALL, NULL);

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

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, SCRIPTDIR_UNINSTALL, NULL);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);

  eam_spawner_run_async (spawner, NULL, run_cb, data);
  g_object_unref (spawner);
}

static void
test_scripts_install_uninstall (void)
{
  setup_filesystem ();
  setup_downloaded_files ();
  setup_gpg ();

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, SCRIPTDIR_INSTALL, NULL);

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

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, SCRIPTDIR_FULL_UPDATE, NULL);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);

  eam_spawner_run_async (spawner, NULL, run_cb, data);
  g_object_unref (spawner);
}

static void
test_scripts_install_update (void)
{
  setup_filesystem ();
  setup_downloaded_files ();
  setup_gpg ();

  /* Pre-install the application */
  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, SCRIPTDIR_INSTALL, NULL);

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

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, SCRIPTDIR_FULL_UPDATE_ROLLBACK, NULL);
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

  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, SCRIPTDIR_FULL_UPDATE, NULL);
  EamSpawner *spawner = eam_spawner_new (scriptdir, script_args);

  eam_spawner_run_async (spawner, NULL, update_cb, data);
  g_object_unref (spawner);
}

static void
test_scripts_install_update_rollback (void)
{
  setup_filesystem ();
  setup_downloaded_files ();
  setup_gpg ();

  /* Pre-install the application */
  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, SCRIPTDIR_INSTALL, NULL);

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
  g_setenv ("EAM_PREFIX", EAM_PREFIX, TRUE);
  g_setenv ("EAM_TMP", EAM_TMP, TRUE);
  g_setenv ("EAM_GPGKEYRING", EAM_GPGKEYRING, TRUE);

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
