/* Copyright 2014 Endless Mobile, Inc. */

#include <eam-pkg.h>

static void
test_pkg_basic (void)
{
  EamPkg *pkg;

  g_assert (!eam_pkg_new_from_keyfile (NULL));
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/pkg/basic", test_pkg_basic);

  return g_test_run ();
}
