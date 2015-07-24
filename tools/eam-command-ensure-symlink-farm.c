#include "config.h"

#include "eam-commands.h"

#include "eam-fs-sanity.h"

#include <stdlib.h>

int
eam_command_ensure_symlink_farm (int argc, char *argv[])
{
  gboolean ret = eam_fs_ensure_symlink_farm ();

  if (ret) {
    return EXIT_SUCCESS;
  } else {
    g_printerr ("Unable to ensure the symlink farm.\n");
    return EXIT_FAILURE;
  }
}
