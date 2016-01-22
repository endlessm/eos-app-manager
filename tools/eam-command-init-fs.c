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

#include "eam-config.h"
#include "eam-fs-utils.h"
#include "eam-utils.h"

#include <stdlib.h>
#include <string.h>
#include <glib.h>

static gboolean opt_verbose = FALSE;

static const GOptionEntry list_apps_entries[] = {
  { "verbose", 'v', 0, G_OPTION_ARG_NONE, &opt_verbose, "Toggle verbose output", NULL },
  { NULL }
};

int
eam_command_init_fs (int argc, char *argv[])
{
  GOptionContext *context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, list_apps_entries, GETTEXT_PACKAGE);

  if (!g_option_context_parse (context, &argc, &argv, NULL)) {
    g_printerr ("Usage: %s init-fs [<options>]\n", eam_argv0);
    return EXIT_FAILURE;
  }

  g_option_context_free (context);

  for (guint i = 0; i < EAM_BUNDLE_DIRECTORY_MAX; i++) {
    if (opt_verbose)
      g_print ("Creating '%s' under '%s'... ",
               eam_fs_get_bundle_system_dir (i),
               eam_config_get_applications_dir ());

    g_autoptr(GError) error = NULL;
    eam_fs_init_bundle_dir (i, &error);
    if (error != NULL) {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

    if (opt_verbose)
      g_print ("ok\n");
  }

  return EXIT_SUCCESS;
}
