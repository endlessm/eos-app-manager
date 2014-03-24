/* Copyright 2014 Endless Mobile, Inc. */

#include <string.h>
#include <glib.h>
#include <eam-pkgdb.h>
#include <eam-version.h>

#define bad_info_1 "[Bundle]"
#define good_info "[Bundle]\nversion = 1"

static void
test_pkg_basic (void)
{
  EamPkg *pkg;
  GKeyFile *keyfile;
  EamPkgVersion *version;

  g_assert_false (eam_pkg_new_from_keyfile (NULL));

  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, bad_info_1, strlen (bad_info_1),
    G_KEY_FILE_NONE, NULL);
  g_assert_null (eam_pkg_new_from_keyfile (keyfile));
  g_key_file_free (keyfile);

  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, good_info, strlen (good_info),
    G_KEY_FILE_NONE, NULL);
  pkg = eam_pkg_new_from_keyfile (keyfile);
  g_assert_nonnull (pkg);
  g_key_file_free (keyfile);

  g_object_get (pkg, "version", &version, NULL);
  g_assert_cmpstr (version->version, ==, "1");
  eam_pkg_version_free (version);
  g_object_unref (pkg);
}

static void
test_pkgdb_basic (void)
{
  EamPkg *pkg, *rpkg;
  EamPkgdb *db;
  GKeyFile *keyfile;

  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, good_info, strlen (good_info),
    G_KEY_FILE_NONE, NULL);
  pkg = eam_pkg_new_from_keyfile (keyfile);
  g_assert_nonnull (pkg);
  g_key_file_free (keyfile);

  db = eam_pkgdb_new ();
  g_assert (eam_pkgdb_add (db, "app01", pkg));
  rpkg = eam_pkgdb_get (db, "app01");
  g_assert (pkg == rpkg);
  g_object_unref (rpkg);
  g_object_unref (pkg);
  g_assert (eam_pkgdb_del (db, "app01"));
  rpkg = eam_pkgdb_get (db, "app01");
  g_assert_null (rpkg);
  g_object_unref (db);
}

static void
test_pkgdb_load (void)
{
  const gchar *appdir = g_test_get_filename (G_TEST_DIST, "appdir", NULL);
  EamPkgdb *db = eam_pkgdb_new_with_appdir (appdir);
  eam_pkgdb_load (db);

  EamPkg *pkg = eam_pkgdb_get (db, "app01");
  g_assert_nonnull (pkg);

  EamPkgVersion *version;
  g_object_get (pkg, "version", &version, NULL);
  g_assert_cmpstr (version->version, ==, "1");
  eam_pkg_version_free (version);
  g_object_unref (pkg);

  g_object_unref (db);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/pkg/basic", test_pkg_basic);
  g_test_add_func ("/pkgdb/basic", test_pkgdb_basic);
  g_test_add_func ("/pkgdb/load", test_pkgdb_load);

  return g_test_run ();
}
