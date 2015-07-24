#include "eam-commands.h"

const EamToolCommand commands[EAM_N_COMMANDS] = {
  [EAM_COMMAND_HELP] = {
    .name = "help",
    .short_desc = "This list of commands",
    .usage = "help [COMMAND]",
    .command_main = eam_command_help,
    .flags = EAM_COMMAND_FLAG_MOST_USED,
  },

  [EAM_COMMAND_VERSION] = {
    .name = "version",
    .short_desc = "Shows the version of the eam tool",
    .usage = "version",
    .command_main = eam_command_version,
    .flags = EAM_COMMAND_FLAG_NONE,
  },

  [EAM_COMMAND_LIST_APPS] = {
    .name = "list-apps",
    .short_desc = "Lists the currently installed applications",
    .usage = "list-apps [--verbose]",
    .command_main = eam_command_list_apps,
    .flags = EAM_COMMAND_FLAG_REQUIRES_CONFIG
           | EAM_COMMAND_FLAG_MOST_USED,
  },

  [EAM_COMMAND_APP_INFO] = {
    .name = "app-info",
    .short_desc = "Shows the bundle information for an application",
    .usage = "app-info APPID",
    .command_main = eam_command_app_info,
    .flags = EAM_COMMAND_FLAG_REQUIRES_CONFIG
           | EAM_COMMAND_FLAG_MOST_USED,
  },

  [EAM_COMMAND_CONFIG] = {
    .name = "config",
    .short_desc = "Display configuration settings",
    .usage = "config [<varname> | list]",
    .command_main = eam_command_config,
    .flags = EAM_COMMAND_FLAG_REQUIRES_CONFIG,
  },

  [EAM_COMMAND_INIT_FS] = {
    .name = "init-fs",
    .short_desc = "Initialize the file system for app bundles",
    .usage = "init-fs [<options>]",
    .command_main = eam_command_init_fs,
    .flags = EAM_COMMAND_FLAG_REQUIRES_ADMIN
           | EAM_COMMAND_FLAG_REQUIRES_CONFIG,
  },

  [EAM_COMMAND_CREATE_SYMLINKS] = {
    .name = "create-symlinks",
    .short_desc = "Create the symlink farm for an application",
    .usage = "create-symlinks <appid>",
    .command_main = eam_command_create_symlinks,
    .flags = EAM_COMMAND_FLAG_REQUIRES_ADMIN,
  },

  [EAM_COMMAND_MIGRATE] = {
    .name = "migrate",
    .short_desc = "Migrates installed apps to a different root",
    .usage = "migrate <appid> <from> <to>",
    .command_main = eam_command_migrate,
    .flags = EAM_COMMAND_FLAG_REQUIRES_ADMIN,
  },

  [EAM_COMMAND_INSTALL] = {
    .name = "install",
    .short_desc = "Installs an application bundle",
    .usage = "install [options] <appid>",
    .command_main = eam_command_install,
    .flags = EAM_COMMAND_FLAG_REQUIRES_CONFIG
           | EAM_COMMAND_FLAG_MOST_USED,
  },

  [EAM_COMMAND_UPDATE] = {
    .name = "update",
    .short_desc = "Updates an application",
    .usage = "update [options] <appid>",
    .command_main = eam_command_update,
    .flags = EAM_COMMAND_FLAG_REQUIRES_CONFIG
           | EAM_COMMAND_FLAG_MOST_USED,
  },

  [EAM_COMMAND_UNINSTALL] = {
    .name = "uninstall",
    .short_desc = "Uninstalls an application",
    .usage = "uninstall [options] <appid>",
    .command_main = eam_command_uninstall,
    .flags = EAM_COMMAND_FLAG_REQUIRES_CONFIG
           | EAM_COMMAND_FLAG_MOST_USED,
  },

  [EAM_COMMAND_ENSURE_SYMLINK_FARM] = {
    .name = "ensure-symlink-farm",
    .short_desc = "Maintains the symlink farm in /endless",
    .usage = "ensure-symlink-farm",
    .command_main = eam_command_ensure_symlink_farm,
    .flags = EAM_COMMAND_FLAG_REQUIRES_CONFIG,
  },
};
