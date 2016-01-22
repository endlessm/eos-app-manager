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
#include "eam-commands.h"

#include <string.h>
#include <stdlib.h>
#include <glib.h>

static char *
most_used_commands (void)
{
  int max_command_len = 0;
  for (int i = 0; i < G_N_ELEMENTS (commands); i++) {
    const EamToolCommand *cmd = &commands[i];

    int command_len = strlen (cmd->name);
    if (command_len > max_command_len)
      max_command_len = command_len;
  }

  GString *buf = g_string_new (NULL);
  g_string_append_printf (buf, "The most commonly used %s commands are:\n", eam_argv0);

  for (int i = 0; i < G_N_ELEMENTS (commands); i++) {
    const EamToolCommand *cmd = &commands[i];

    if ((cmd->flags & EAM_COMMAND_FLAG_MOST_USED) != 0) {
      g_string_append_printf (buf, "  %-*s\t%s %s\n",
                              max_command_len, cmd->name,
                              cmd->short_desc,
                              (cmd->flags & EAM_COMMAND_FLAG_REQUIRES_ADMIN) != 0
                                ? "(requires admin)" : "");
    }
  }

  g_string_append_c (buf, '\n');

  return g_string_free (buf, FALSE);
}

static char *
remaining_commands (void)
{
  int max_command_len = 0;
  for (int i = 0; i < G_N_ELEMENTS (commands); i++) {
    const EamToolCommand *cmd = &commands[i];

    int command_len = strlen (cmd->name);
    if (command_len > max_command_len)
      max_command_len = command_len;
  }

  GString *buf = g_string_new (NULL);
  g_string_append_printf (buf, "Other available %s commands:\n", eam_argv0);

  g_string_append (buf, "  ");

  int i = 0, col_num = 0;
  while (i < G_N_ELEMENTS (commands)) {
    const EamToolCommand *cmd = &commands[i];
    if ((cmd->flags & EAM_COMMAND_FLAG_MOST_USED) == 0) {
      g_string_append_printf (buf, "%-*s\t", max_command_len, cmd->name);
      col_num += 1;
    }

    if (col_num % 3 == 0) {
      g_string_append_c (buf, '\n');
      g_string_append (buf, "  ");
      col_num = 0;
    }

    i += 1;
  }

  g_string_append_c (buf, '\n');

  return g_string_free (buf, FALSE);
}

int
eam_command_help (int argc,
                  char *argv[])
{
  if (argc < 2) {
    g_autofree char *most_used_commands_str = most_used_commands ();
    g_autofree char *remaining_commands_str = remaining_commands ();

    g_print ("Usage: %s <command> [<args>]\n"
             "\n"
             "%s"
             "\n"
             "%s"
             "\n",
             eam_argv0,
             most_used_commands_str,
             remaining_commands_str);
  }
  else {
    const char *command = argv[1];

    for (int i = 0; i < G_N_ELEMENTS (commands); i++) {
      if (strcmp (commands[i].name, command) == 0) {
        g_print ("Usage: %s %s\n"
                 "\n"
                 "  %s\t%s %s\n",
                 eam_argv0,
                 commands[i].usage,
                 commands[i].name, commands[i].short_desc,
                 (commands[i].flags & EAM_COMMAND_FLAG_REQUIRES_ADMIN) != 0
                   ? "(requires admin)" : "");
      }
    }
  }

  return EXIT_SUCCESS;
}
