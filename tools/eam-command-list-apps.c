#include "config.h"

#include "eam-commands.h"

#include "eam-config.h"
#include "eam-fs-sanity.h"
#include "eam-utils.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <gio/gio.h>

static gboolean list_apps_verbose = FALSE;

static const GOptionEntry list_apps_entries[] = {
  { "verbose", 'v', 0, G_OPTION_ARG_NONE, &list_apps_verbose, NULL, NULL },
  { NULL }
};

int
eam_command_list_apps (int argc, char *argv[])
{
  GOptionContext *context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, list_apps_entries, GETTEXT_PACKAGE);

  if (!g_option_context_parse (context, &argc, &argv, NULL)) {
    g_printerr ("Usage: %s list-apps [--verbose]\n", eam_argv0);
    return EXIT_FAILURE;
  }

  g_option_context_free (context);

  const char *appdir = eam_config_get_applications_dir ();
  g_assert (appdir != NULL);

  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = g_file_new_for_path (appdir);
  g_autoptr(GFileEnumerator) enumerator =
    g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME,
                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                               NULL, &error);

  if (error != NULL) {
    g_printerr ("Failed to enumerate application directory '%s': %s\n",
                appdir, error->message);
    return EXIT_FAILURE;
  }

  while (TRUE) {
    GFileInfo *child_info = NULL;
    GFile *child = NULL;

    if (!g_file_enumerator_iterate (enumerator, &child_info, &child, NULL, &error)) {
      g_printerr ("Failed to iterate: %s\n", error->message);
      break;
    }

    if (child_info == NULL)
      break;

    g_autofree char *child_path = g_file_get_path (child);
    if (!eam_fs_is_app_dir (child_path))
      continue;

    if (!list_apps_verbose) {
      g_print ("%s\n", g_file_info_get_name (child_info));
    }
    else {
      g_autoptr(GKeyFile) keyfile =
        eam_utils_load_app_info (appdir, g_file_info_get_name (child_info));

      if (keyfile == NULL) {
        g_printerr ("*** Unable to load bundle info for '%s' ***\n",
                    g_file_info_get_name (child_info));
        continue;
      }

      g_autofree char *appid = g_key_file_get_string (keyfile, "Bundle", "app_id", NULL);
      g_print ("%s─┬─path───%s\n", appid, child_path);

      g_autofree char *version = g_key_file_get_string (keyfile, "Bundle", "version", NULL);

      if (g_key_file_has_group (keyfile, "External")) {
	g_print ("%*s ├─version───%s\n",
		 (int) strlen (appid), " ", version != NULL ? version : "<none>");

        g_autofree char *ext_url = g_key_file_get_string (keyfile, "External", "url", NULL);
        g_autofree char *ext_sum = g_key_file_get_string (keyfile, "External", "sha256sum", NULL);

        g_print ("%*s └─external─┬─url───%s\n"
                 "%*s            └─sha256───%s\n\n",
                 (int) strlen (appid), " ", ext_url != NULL ? ext_url : "<none>",
                 (int) strlen (appid), " ", ext_sum != NULL ? ext_sum : "<none>");
      }
      else {
	g_print ("%*s └─version───%s\n\n",
		 (int) strlen (appid), " ", version != NULL ? version : "<none>");
      }
    }
  }

  return EXIT_SUCCESS;
}
