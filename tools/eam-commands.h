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

#include <glib.h>

typedef struct {
  const char *name;
  const char *short_desc;
  const char *usage;

  int (* command_main) (int argc, char *argv[]);

  unsigned int flags;
} EamToolCommand;

enum {
  EAM_COMMAND_FLAG_NONE = 0,
  EAM_COMMAND_FLAG_REQUIRES_ADMIN = 1 << 0,
  EAM_COMMAND_FLAG_REQUIRES_CONFIG = 1 << 1,
  EAM_COMMAND_FLAG_MOST_USED = 1 << 2
};

enum {
  EAM_COMMAND_HELP,
  EAM_COMMAND_VERSION,
  EAM_COMMAND_LIST_APPS,
  EAM_COMMAND_APP_INFO,
  EAM_COMMAND_CONFIG,
  EAM_COMMAND_INIT_FS,
  EAM_COMMAND_CREATE_SYMLINKS,
  EAM_COMMAND_MIGRATE,
  EAM_COMMAND_INSTALL,
  EAM_COMMAND_UPDATE,
  EAM_COMMAND_UNINSTALL,
  EAM_COMMAND_ENSURE_SYMLINK_FARM,

  EAM_N_COMMANDS
};

extern const EamToolCommand commands[EAM_N_COMMANDS];

extern char *eam_argv0;

extern int eam_command_help (int argc, char *argv[]);
extern int eam_command_version (int argc, char *argv[]);
extern int eam_command_list_apps (int argc, char *argv[]);
extern int eam_command_app_info (int argc, char *argv[]);
extern int eam_command_config (int argc, char *argv[]);
extern int eam_command_init_fs (int argc, char *argv[]);
extern int eam_command_create_symlinks (int argc, char *argv[]);
extern int eam_command_install (int argc, char *argv[]);
extern int eam_command_migrate (int argc, char *argv[]);
extern int eam_command_update (int argc, char *argv[]);
extern int eam_command_uninstall (int argc, char *argv[]);
extern int eam_command_ensure_symlink_farm (int argc, char *argv[]);
