/* Copyright 2014 Endless Mobile, Inc. */

#include <eam-spawner.h>

static void
run_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  GError *error = NULL;
  eam_spawner_run_finish (EAM_SPAWNER (source), res, &error);
  if (error) {
    g_print ("Error: %s\n", error->message);
    g_clear_error (&error);
  }
  g_main_loop_quit (data);
}

static void
test_spawner_run (void)
{
  const gchar *scriptdir = g_test_get_filename (G_TEST_DIST, "scriptdir", NULL);
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  EamSpawner *spawner = eam_spawner_new (scriptdir, NULL);

  eam_spawner_run_async (spawner, NULL, run_cb, loop);
  g_object_unref (spawner);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/spawner/run", test_spawner_run);

  return g_test_run ();
}
