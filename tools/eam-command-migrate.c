#include "config.h"

#include "eam-commands.h"

#include "eam-fs.h"
#include "eam-utils.h"

#include <glib.h>
#include <stdlib.h>

int
eam_command_migrate (int argc, char *argv[])
{
  if (argc != 4) {
    g_printerr ("Usage: %s migrate APPID FROM TO\n", eam_argv0);
    return EXIT_FAILURE;
  }

  const char *appid = argv[1];
  const char *from = argv[2];
  const char *to = argv[3];

  if (!eam_utils_app_is_installed (from, appid)) {
    g_printerr ("Application '%s' is not installed in '%s'.\n", appid, from);
    return EXIT_FAILURE;
  }

  if (eam_utils_app_is_installed (to, appid)) {
    g_printerr ("Application '%s' is already installed in '%s'.\n", appid, to);
    return EXIT_SUCCESS;
  }

  /* Deploy app */
  if (!eam_fs_deploy_app (from, to, appid)) {
    g_printerr ("Could not move application '%s' from '%s' to '%s'.\n",
                appid, from, to);
    return EXIT_FAILURE;
  }

  /* Recreate symlinks and update the system */
  if (!eam_fs_create_symlinks (to, appid)) {
    g_printerr ("Could not recreate symlinks for app '%s'.\n", appid);
    return EXIT_FAILURE;
  }

  eam_fs_prune_symlinks (from, appid);
  eam_utils_compile_python (to, appid);
  eam_utils_update_desktop (to);

  return EXIT_SUCCESS;
}
