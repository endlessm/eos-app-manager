/* Copyright 2014 Endless Mobile, Inc. */

#include <string.h>
#include <glib.h>
#include <eam-pkgdb.h>
#include <eam-version.h>

#define bad_info_1 "[Bundle]"
#define good_info "[Bundle]\nappId = app01\nappName= application one\ncodeVersion = 1\n"
#define good_json "{ \"appId\": \"com.application.id1\", \"appName\": \"App Name 1\", \"codeVersion\": \"1.1\" }"

static void
test_pkg_basic (void)
{
  EamPkg *pkg, *cpkg;
  GKeyFile *keyfile;

  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, bad_info_1, strlen (bad_info_1),
    G_KEY_FILE_NONE, NULL);
  g_assert_null (eam_pkg_new_from_keyfile (keyfile, NULL));
  g_key_file_free (keyfile);

  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, good_info, strlen (good_info),
    G_KEY_FILE_NONE, NULL);
  pkg = eam_pkg_new_from_keyfile (keyfile, NULL);
  g_assert_nonnull (pkg);
  g_key_file_free (keyfile);

  g_assert_cmpstr (eam_pkg_get_version (pkg)->version, ==, "1");

  cpkg = eam_pkg_copy (pkg);
  g_assert_cmpstr (eam_pkg_get_id (pkg), ==, eam_pkg_get_id (cpkg));
  g_assert_cmpstr (eam_pkg_get_name (pkg), ==, eam_pkg_get_name (cpkg));
  g_assert_cmpstr (eam_pkg_get_version (pkg)->version, ==,
                   eam_pkg_get_version (cpkg)->version);

  eam_pkg_free (cpkg);
  eam_pkg_free (pkg);
}

static void
test_pkg_json (void)
{
  JsonParser *parser = json_parser_new ();
  g_assert (json_parser_load_from_data (parser, good_json, -1, NULL));

  JsonNode *node = json_parser_get_root (parser);
  g_assert_nonnull (node);

  JsonObject *json = json_node_get_object (node);
  g_assert_nonnull (json);

  EamPkg *pkg = eam_pkg_new_from_json_object (json, NULL);
  g_assert_nonnull (pkg);

  g_assert_cmpstr (eam_pkg_get_version (pkg)->version, ==, "1.1");
  eam_pkg_free (pkg);
  g_object_unref (parser);
}

static void
test_pkgdb_basic (void)
{
  EamPkg *pkg;
  const EamPkg *rpkg;
  EamPkgdb *db;
  GKeyFile *keyfile;

  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, good_info, strlen (good_info),
    G_KEY_FILE_NONE, NULL);
  pkg = eam_pkg_new_from_keyfile (keyfile, NULL);
  g_assert_nonnull (pkg);
  g_key_file_free (keyfile);

  db = eam_pkgdb_new ();

  g_assert (eam_pkgdb_add (db, "app01", pkg));
  rpkg = eam_pkgdb_get (db, "app01");
  g_assert (pkg == rpkg);

  g_assert (eam_pkgdb_del (db, "app01"));
  rpkg = eam_pkgdb_get (db, "app01");
  g_assert_null (rpkg);
  g_object_unref (db);
}

static void
load_tests (EamPkgdb *db)
{
  const EamPkg *pkg = eam_pkgdb_get (db, "app01");
  g_assert_nonnull (pkg);

  g_assert_cmpstr (eam_pkg_get_version (pkg)->version, ==, "1");
}

static void
test_pkgdb_load (void)
{
  const gchar *appdir = g_test_get_filename (G_TEST_DIST, "appdir", NULL);
  EamPkgdb *db = eam_pkgdb_new_with_appdir (appdir);
  eam_pkgdb_load (db);

  load_tests (db);

  g_object_unref (db);
}

static void
load_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  eam_pkgdb_load_finish (EAM_PKGDB (source), res);
  g_main_loop_quit (data);
}

static void
test_pkgdb_load_async (void)
{
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  const gchar *appdir = g_test_get_filename (G_TEST_DIST, "appdir", NULL);
  EamPkgdb *db = eam_pkgdb_new_with_appdir (appdir);

  eam_pkgdb_load_async (db, NULL, load_cb, loop);
  g_main_loop_run (loop);

  load_tests (db);

  g_main_loop_unref (loop);
  g_object_unref (db);
}


int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/pkg/basic", test_pkg_basic);
  g_test_add_func ("/pkg/json", test_pkg_json);
  g_test_add_func ("/pkgdb/basic", test_pkgdb_basic);
  g_test_add_func ("/pkgdb/load", test_pkgdb_load);
  g_test_add_func ("/pkgdb/load_async", test_pkgdb_load_async);

  return g_test_run ();
}
