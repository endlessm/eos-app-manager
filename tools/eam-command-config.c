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
#include "eam-utils.h"

#include <stdlib.h>
#include <string.h>
#include <glib.h>

static void
print_all (void)
{
  g_print ("eam─┬─directories─┬─apps root───%s\n"
           "    │             ├─cache dir───%s\n"
           "    │             ├─primary storage───%s\n"
           "    │             ├─secondary storage───%s\n"
           "    │             └─gpg keyring───%s\n"
           "    ├─repository─┬─server url───%s\n"
           "    │            ├─api version───%s\n"
           "    │            └─delta updates───%s\n"
           "    └─daemon───inactivity timeout───%u\n",
           eam_config_get_applications_dir (),
           eam_config_get_cache_dir (),
           eam_config_get_primary_storage (),
           eam_config_get_secondary_storage (),
           eam_config_get_gpg_keyring (),
           eam_config_get_server_url (),
           eam_config_get_api_version (),
           eam_config_get_enable_delta_updates () ? "true" : "false",
           eam_config_get_inactivity_timeout ());
}

int
eam_command_config (int argc, char *argv[])
{
  if (argc == 1) {
    print_all ();
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "list") == 0) {
    g_print ("ApplicationsDir\n"
             "CacheDir\n"
             "PrimaryStorage\n"
             "SecondaryStorage\n"
             "GpgKeyring\n"
             "ServerURL\n"
             "ProtocolVersion\n"
             "DeltaUpdates\n"
             "InactivityTimeout\n");
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "set") == 0) {
    if (argc != 4) {
      g_printerr ("Usage: %s config set <key> <value>\n", eam_argv0);
      return EXIT_FAILURE;
    }

    if (!eam_utils_can_modify_configuration (getuid ())) {
      g_printerr ("You need administrator privileges to modify the app manager configuration\n");
      return EXIT_FAILURE;
    }

    if (!eam_config_set_key (argv[2], argv[3])) {
      g_printerr ("Could not set configuration key '%s'\n", argv[2]);
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "reset") == 0) {
    if (argc != 3) {
      g_printerr ("Usage: %s config reset <key>\n", eam_argv0);
      return EXIT_FAILURE;
    }

    if (!eam_utils_can_modify_configuration (getuid ())) {
      g_printerr ("You need administrator privileges to modify the app manager configuration\n");
      return EXIT_FAILURE;
    }

    if (!eam_config_reset_key (argv[2])) {
      g_printerr ("Could not reset configuration key '%s'\n", argv[2]);
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "ApplicationsDir") == 0) {
    g_print ("%s\n", eam_config_get_applications_dir ());
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "CacheDir") == 0) {
    g_print ("%s\n", eam_config_get_cache_dir ());
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "PrimaryStorage") == 0) {
    g_print ("%s\n", eam_config_get_primary_storage ());
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "SecondaryStorage") == 0) {
    g_print ("%s\n", eam_config_get_secondary_storage ());
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "GpgKeyring") == 0) {
    g_print ("%s\n", eam_config_get_gpg_keyring ());
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "ServerURL") == 0) {
    g_print ("%s\n", eam_config_get_server_url ());
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "ProtocolVersion") == 0) {
    g_print ("%s\n", eam_config_get_api_version ());
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "DeltaUpdates") == 0) {
    g_print ("%s\n", eam_config_get_enable_delta_updates () ? "true" : "false");
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "InactivityTimeout") == 0) {
    g_print ("%u\n", eam_config_get_inactivity_timeout ());
    return EXIT_SUCCESS;
  }

  g_printerr ("Unknown configuration key '%s'\n", argv[1]);

  return EXIT_FAILURE;
}
