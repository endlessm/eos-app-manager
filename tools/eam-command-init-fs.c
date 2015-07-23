#include "config.h"

#include "eam-commands.h"

#include "eam-config.h"
#include "eam-fs.h"
#include "eam-utils.h"

#include <stdlib.h>
#include <string.h>
#include <glib.h>

static gboolean opt_verbose = FALSE;
static char *opt_prefix = NULL;

static const GOptionEntry list_apps_entries[] = {
  { "verbose", 'v', 0, G_OPTION_ARG_NONE, &opt_verbose, "Toggle verbose output", NULL },
  { "prefix", 0, 0, G_OPTION_ARG_STRING, &opt_prefix, "Prefix for the initialization", "DIRECTORY" },
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

  if (opt_prefix == NULL)
    opt_prefix = g_strdup (eam_config_get_applications_dir ());

  for (guint i = 0; i < EAM_BUNDLE_DIRECTORY_MAX; i++) {
    if (opt_verbose)
      g_print ("Creating '%s' under '%s'... ", eam_fs_get_bundle_system_dir (i), opt_prefix);

    g_autoptr(GError) error = NULL;
    eam_fs_init_bundle_dir (opt_prefix, i, &error);
    if (error != NULL) {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

    if (opt_verbose)
      g_print ("ok\n");
  }

  return EXIT_SUCCESS;
}
