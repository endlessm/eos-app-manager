#include "config.h"

#include "eam-commands.h"

#include "eam-fs-utils.h"
#include "eam-utils.h"

#include <stdlib.h>

int
eam_command_ensure_symlink_farm (int argc, char *argv[])
{
  if (!eam_fs_ensure_symlink_farm ()) {
    g_printerr ("Unable to ensure the symlink farm.\n");
    return EXIT_FAILURE;
  }

  if (!eam_utils_update_desktop ()) {
    g_printerr ("Unable to update desktop databases.\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
