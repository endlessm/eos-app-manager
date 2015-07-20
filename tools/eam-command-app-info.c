#include "config.h"

#include "eam-commands.h"

#include "eam-config.h"
#include "eam-fs-sanity.h"
#include "eam-utils.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <gio/gio.h>

int
eam_command_app_info (int argc, char *argv[])
{
  if (argc == 1) {
    g_printerr ("Usage: %s app-info <appid>\n", eam_argv0);
    return EXIT_FAILURE;
  }

  const char *appdir = eam_config_get_applications_dir ();
  g_assert (appdir != NULL);

  g_autofree char *child_path = g_build_filename (appdir, argv[1], NULL);

  if (!eam_fs_is_app_dir (child_path)) {
    g_printerr ("No application '%s' found.\n", argv[1]);
    return EXIT_FAILURE;
  }

  g_autoptr(GKeyFile) keyfile = eam_utils_load_app_info (appdir, argv[1]);
  if (keyfile == NULL) {
    g_printerr ("Unable to load bundle info for '%s'.\n", argv[1]);
    return EXIT_FAILURE;
  }

  g_autofree char *appid = g_key_file_get_string (keyfile, "Bundle", "app_id", NULL);

    g_print ("%s─┬─path───%s\n", appid, child_path);

  if (g_key_file_has_group (keyfile, "External")) {
    g_autofree char *ext_url = g_key_file_get_string (keyfile, "External", "url", NULL);
    g_autofree char *ext_sum = g_key_file_get_string (keyfile, "External", "sha256sum", NULL);

    g_print ("%*s └─external─┬─url───%s\n"
             "%*s            └─sha256─── %s\n",
             (int) strlen (appid), " ",
             ext_url != NULL ? ext_url : "<none>",
             (int) strlen (appid), " ",
             ext_sum != NULL ? ext_sum : "<none>");
  }
  else {
    g_print ("%*s └─no external scripts\n", (int) strlen (appid), " ");
  }

  return EXIT_SUCCESS;
}
