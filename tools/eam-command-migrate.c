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

#include "eam-fs-utils.h"
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
  if (!eam_fs_deploy_app (from, to, appid, NULL)) {
    g_printerr ("Could not move application '%s' from '%s' to '%s'.\n",
                appid, from, to);
    return EXIT_FAILURE;
  }

  /* Recreate symlinks and update the system */
  eam_fs_prune_symlinks (from, appid);
  if (!eam_fs_create_symlinks (to, appid)) {
    g_printerr ("Could not recreate symlinks for app '%s'.\n", appid);
    return EXIT_FAILURE;
  }

  /* Run all update hooks, but pass back failures */
  int ret = EXIT_SUCCESS;
  if (!eam_utils_compile_python (to, appid)) {
    g_printerr ("Could not compile python objects for app '%s'.\n", appid);
    ret = EXIT_FAILURE;
  }
  if (!eam_utils_update_desktop ()) {
    g_printerr ("Could not update desktop caches.\n");
    ret = EXIT_FAILURE;
  }

  return ret;
}
