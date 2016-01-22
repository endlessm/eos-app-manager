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
