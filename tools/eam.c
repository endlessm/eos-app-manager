/* eam: Command line tool for eos-app-manager
 *
 * This file is part of eos-app-manager.
 * Copyright 2014  Endless Mobile Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>
#include <sys/types.h>

#include "eam-commands.h"

#include "eam-utils.h"

#define CONFIG_FILE     SYSCONFDIR "/eos-app-manager/eos-app-manager.ini"

char *eam_argv0;

int
main (int argc, char *argv[])
{
  setlocale (LC_ALL, "");
  textdomain (GETTEXT_PACKAGE);
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  eam_argv0 = g_path_get_basename (argv[0]);

  if (argc == 1) {
    commands[EAM_COMMAND_HELP].command_main (0, NULL);
    return EXIT_FAILURE;
  }

  if (argc == 2 &&
      (strcmp (argv[1], "--help") == 0 ||
       strcmp (argv[1], "-h") == 0 ||
       strcmp (argv[1], "-?") == 0))
    return commands[EAM_COMMAND_HELP].command_main (0, NULL);

  if (argc == 2 &&
      strcmp (argv[1], "--version") == 0)
    return commands[EAM_COMMAND_VERSION].command_main (0, NULL);

  for (int i = 0; i < G_N_ELEMENTS (commands); i++) {
    if (strcmp (argv[1], commands[i].name) == 0) {
      if ((commands[i].flags & EAM_COMMAND_FLAG_REQUIRES_ADMIN) != 0) {
        if (!eam_utils_check_unix_permissions (geteuid ())) {
          g_printerr ("The '%s' command requires admin capabilities.\n", commands[i].name);
          return EXIT_FAILURE;
        }
      }

      if ((commands[i].flags & EAM_COMMAND_FLAG_REQUIRES_CONFIG) != 0) {
        if (!g_file_test (CONFIG_FILE, G_FILE_TEST_EXISTS)) {
          g_printerr ("The '%s' command requires the config file in '%s'.\n",
                      commands[i].name,
                      CONFIG_FILE);
          return EXIT_FAILURE;
        }
      }

      char **pruned_argv = NULL;
      int pruned_argc = 0;
      if (argc > 1) {
        pruned_argc = argc - 1;
        pruned_argv = argv + 1;
      }

      g_assert (commands[i].command_main != NULL);

      return commands[i].command_main (pruned_argc, pruned_argv);
    }
  }

  g_printerr ("Unknown command '%s'\n", argv[1]);
  return EXIT_FAILURE;
}
