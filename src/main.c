#include <stdlib.h>

#include "eam-pkgdb.h"

int
main (int argc, gchar **argv)
{
  EamPkgdb *db = eam_pkgdb_new ();
  EamPkg *pkg = eam_pkg_new_from_keyfile (NULL);

  g_object_unref (db);
  g_object_unref (pkg);

  return EXIT_SUCCESS;
}
