/* Copyright 2014 Endless Mobile, Inc. */

#include <string.h>
#include <glib.h>
#include <eam-pkg.h>

#define bad_info_1 "[Bundle]"
#define good_info "[Bundle]\nversion = 1"

static void
test_pkg_basic (void)
{
  EamPkg *pkg;
  GKeyFile *keyfile;

  g_assert (!eam_pkg_new_from_keyfile (NULL));

  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, bad_info_1, strlen (bad_info_1),
    G_KEY_FILE_NONE, NULL);
  g_assert (!eam_pkg_new_from_keyfile (keyfile));
  g_key_file_free (keyfile);

  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, good_info, strlen (good_info),
    G_KEY_FILE_NONE, NULL);
  pkg = eam_pkg_new_from_keyfile (keyfile);
  g_assert (pkg);
  g_key_file_free (keyfile);
  g_object_unref (pkg);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/pkg/basic", test_pkg_basic);

  return g_test_run ();
}
