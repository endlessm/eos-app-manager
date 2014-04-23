/* Copyright 2014 Endless Mobile, Inc. */

#include <glib.h>
#include <string.h>
#include <eam-config.h>
#include <eam-updates.h>

#define config "[eam]\nappdir = /endless\n" \
  "downloaddir=/tmp\n" \
  "serveraddress = http://people.igalia.com/vjaquez/eos/\n" \
  "protocolversion = v1"

#define updates_1 "[ { \"appId\": \"com.application.id1\", \"appName\": \"App Name 1\", \"codeVersion\": \"1.1\" } ]"
#define updates_2 "{ [ { \"appId\": \"com.application.id1\", \"appName\": \"App Name 1\", \"codeVersion\": \"1.2\" } ] }"

#define DL 0

#if DL
/* @TODO: use a #GTestCase for this fixture */
static void
load_config ()
{
  EamConfig *cfg = eam_config_get ();
  GKeyFile *keyfile = g_key_file_new ();

  g_key_file_load_from_data (keyfile, config, strlen (config), G_KEY_FILE_NONE, NULL);
  eam_config_load (cfg, keyfile);
  g_key_file_unref (keyfile);
}

static void
dl_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  eam_updates_fetch_finish (EAM_UPDATES (source), res, NULL);
  g_main_loop_quit (data);
}

static void
test_updates_dl (void)
{
  load_config ();

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  EamUpdates *updates = eam_updates_new ();
  g_assert_nonnull (updates);

  eam_updates_fetch_async (updates, NULL, dl_cb, loop);
  g_main_loop_run (loop);

  GError *error = NULL;
  gboolean ret = eam_updates_parse (updates, &error);
  if (error) {
    g_printerr ("ERROR: %s\n", error->message);
    g_clear_error (&error);
  }

  g_assert_null (error);
  g_assert (ret);

  g_main_loop_unref (loop);
  g_object_unref (updates);
}
#endif

static void
test_updates_load (void)
{
  JsonParser *parser = json_parser_new ();
  g_assert (json_parser_load_from_data (parser, updates_1, -1, NULL));

  JsonNode *node = json_parser_get_root (parser);
  g_assert_nonnull (node);

  EamUpdates *updates = eam_updates_new ();

  g_assert (eam_updates_load (updates, node, NULL));

  g_object_unref (updates);
  g_object_unref (parser);
}

static void
test_updates_filter (void)
{
  const gchar *avail = g_test_get_filename (G_TEST_DIST, "data",
     "available.json", NULL);
  JsonParser *parser = json_parser_new ();
  g_assert (json_parser_load_from_file (parser, avail, NULL));

  JsonNode *node = json_parser_get_root (parser);
  g_assert_nonnull (node);

  EamUpdates *updates = eam_updates_new ();
  g_assert (eam_updates_load (updates, node, NULL));

  const gchar *appdir = g_test_get_filename (G_TEST_DIST, "appdir", NULL);
  EamPkgdb *db = eam_pkgdb_new_with_appdir (appdir);
  eam_pkgdb_load (db, NULL);

  eam_updates_filter (updates, db);

  GList *list;
  EamPkg *pkg;
  list = eam_updates_get_upgradables (updates);
  g_assert_nonnull (list);
  pkg = list->data;
  g_assert_nonnull (pkg);
  g_assert_cmpstr (eam_pkg_get_id (pkg), ==, "com.application.id2");
  g_assert_null (list->next);

  list = eam_updates_get_installables (updates);
  g_assert_nonnull (list);
  pkg = list->data;
  g_assert_nonnull (pkg);
  g_assert_cmpstr (eam_pkg_get_id (pkg), ==, "com.application.id3");
  g_assert_null (list->next);

  g_object_unref (updates);
  g_object_unref (parser);
  g_object_unref (db);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  #if DL
  /* comment it out because got a strange segmentation fault in
   * iconv_open() */
  g_test_add_func ("/updates/dl", test_updates_dl);
  #endif

  g_test_add_func ("/updates/load", test_updates_load);
  g_test_add_func ("/updates/filter", test_updates_filter);

  return g_test_run ();
}
