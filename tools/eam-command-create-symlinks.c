#include "config.h"

#include "eam-commands.h"

#include "eam-config.h"
#include "eam-fs-sanity.h"
#include "eam-utils.h"

#include <stdlib.h>
#include <glib.h>

static char *opt_prefix;
static char *opt_migrate_to;
static char **opt_appid;

static const GOptionEntry opt_entries[] = {
  { "prefix", 0, 0, G_OPTION_ARG_FILENAME, &opt_prefix, "Prefix to use when creating symlinks", "DIRECTORY" },
  { "migrate-to", 0, 0, G_OPTION_ARG_FILENAME, &opt_migrate_to, "Migrate to another path", "PATH" },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &opt_appid, "Application id", "APPID" },
  { NULL },
};

int
eam_command_create_symlinks (int argc, char *argv[])
{
  GOptionContext *context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, opt_entries, GETTEXT_PACKAGE);

  if (!g_option_context_parse (context, &argc, &argv, NULL)) {
    g_printerr ("Usage: %s create-symlinks APPID\n", eam_argv0);
    return EXIT_FAILURE;
  }

  g_option_context_free (context);

  if (opt_appid == NULL || g_strv_length (opt_appid) != 1) {
    g_printerr ("Usage: %s create-symlinks APPID\n", eam_argv0);
    return EXIT_FAILURE;
  }

  if (opt_prefix == NULL)
    opt_prefix = g_strdup (eam_config_appdir ());

  const char *appid = opt_appid[0];

  if (opt_migrate_to != NULL) {
    if (!eam_fs_create_symlinks (opt_migrate_to, appid)) {
      g_printerr ("Unable to migrate symlinks from '%s' to '%s' for app '%s'.\n",
                  opt_prefix,
                  opt_migrate_to,
                  appid);
      return EXIT_FAILURE;
    }

    eam_fs_prune_symlinks (opt_prefix, appid);
  }
  else {
    if (!eam_fs_create_symlinks (opt_prefix, appid)) {
      g_printerr ("Unable to create symlinks in '%s' for app '%s'.\n",
                  opt_prefix,
                  appid);
      return EXIT_FAILURE;
    }
  }

  eam_utils_update_desktop (opt_migrate_to == NULL ? opt_prefix : opt_migrate_to);

  return EXIT_SUCCESS;
}
