#include "config.h"
#include "eam-commands.h"

#include <stdlib.h>

int
eam_command_version (int argc,
                     char *argv[])
{
  g_print ("%s version %s\n", eam_argv0, PACKAGE_VERSION);

  return EXIT_SUCCESS;
}
