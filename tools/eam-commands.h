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
