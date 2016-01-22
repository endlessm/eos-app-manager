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
#include "eam-fs-utils.h"
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

typedef struct _AppFile {
  GFileInfo *child_info;
  GFile *child;
} AppFile;

static gint
app_file_compare (gconstpointer a, gconstpointer b)
{
  AppFile *app_file_a = (AppFile *) a;
  AppFile *app_file_b = (AppFile *) b;
  return g_utf8_collate (g_file_info_get_name (app_file_a->child_info),
                         g_file_info_get_name (app_file_b->child_info));
}

static void
app_file_print (gpointer data, gpointer user_data)
{
  AppFile *app_file = data;
  GFileInfo *child_info = app_file->child_info;
  GFile *child = app_file->child;

  const char *appdir = user_data;

  g_autoptr(GFile) target_dir = NULL;
  if (g_file_info_get_is_symlink (child_info))
    target_dir = g_file_resolve_relative_path (
        child,
        g_file_info_get_symlink_target (child_info));
  else
    target_dir = g_object_ref (child);

  g_autofree char *child_path = g_file_get_path (target_dir);

  if (!eam_fs_is_app_dir (child_path))
    return;

  if (!list_apps_verbose) {
    g_print ("%s\n", g_file_info_get_name (child_info));
  }
  else {
    g_autoptr(GKeyFile) keyfile =
      eam_utils_load_app_info (appdir, g_file_info_get_name (child_info));

    if (keyfile == NULL) {
      g_printerr ("*** Unable to load bundle info for '%s' ***\n",
                  g_file_info_get_name (child_info));
      return;
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

static void
app_file_free (AppFile *app_file)
{
  g_object_unref (app_file->child_info);
  g_object_unref (app_file->child);
  g_free (app_file);
}

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
    g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                     G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
                                     G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                               NULL, &error);

  if (error != NULL) {
    g_printerr ("Failed to enumerate application directory '%s': %s\n",
                appdir, error->message);
    return EXIT_FAILURE;
  }

  GList* file_list = NULL;

  while (TRUE) {
    AppFile *app_file = g_new0 (AppFile, 1);
    GFileInfo *child_info = NULL;
    GFile *child = NULL;

    if (!g_file_enumerator_iterate (enumerator, &child_info, &child, NULL, &error)) {
      g_printerr ("Failed to iterate: %s\n", error->message);
      break;
    }

    if (child_info == NULL)
      break;

    app_file->child_info = g_object_ref (child_info);
    app_file->child = g_object_ref (child);
    
    file_list = g_list_insert_sorted (file_list,
                                      app_file,
                                      app_file_compare);
  }

  g_list_foreach (file_list, app_file_print, (char *) appdir);

  g_list_free_full (file_list, (GDestroyNotify) app_file_free);
  
  return EXIT_SUCCESS;
}
