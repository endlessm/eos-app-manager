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
#include "eam-error.h"
#include "eam-fs-utils.h"
#include "eam-uninstall.h"
#include "eam-utils.h"

#include "eos-app-manager-service.h"
#include "eos-app-manager-transaction.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>

#include <glib.h>
#include <gio/gio.h>

static gboolean opt_force;
static char *opt_prefix;
static char **opt_appid;

static const GOptionEntry install_entries[] = {
 { "force", 'f', 0, G_OPTION_ARG_NONE, &opt_force, "Forces the uninstallation", NULL },
 { "prefix", 0, 0, G_OPTION_ARG_FILENAME, &opt_prefix, "Prefix for the uninstallation", "DIRECTORY" },
 { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &opt_appid, "Application id", "APPID" },
 { NULL },
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC (EosAppManager, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EosAppManagerTransaction, g_object_unref)

int
eam_command_uninstall (int argc, char *argv[])
{
  g_autoptr(GOptionContext) context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, install_entries, GETTEXT_PACKAGE);

  if (!g_option_context_parse (context, &argc, &argv, NULL)) {
    g_printerr ("Usage: %s uninstall [-f] APPID\n", eam_argv0);
    return EXIT_FAILURE;
  }

  if (opt_appid == NULL || g_strv_length (opt_appid) != 1) {
    g_printerr ("Usage: %s uninstall [-f] APPID\n", eam_argv0);
    return EXIT_FAILURE;
  }

  const char *appid = opt_appid[0];

  /* If we are being called by a privileged user, then we bypass the
   * daemon entirely, because we have enough privileges.
   */
  if (eam_utils_can_touch_applications_dir (geteuid ())) {
    g_autoptr(EamUninstall) uninstall = (EamUninstall *) eam_uninstall_new (appid);

    eam_uninstall_set_prefix (uninstall, opt_prefix);
    eam_uninstall_set_force (uninstall, opt_force);

    g_autoptr(GError) error = NULL;
    eam_transaction_run_sync (EAM_TRANSACTION (uninstall), NULL, &error);
    if (error != NULL) {
      g_printerr ("Uninstallation failed: %s\n", error->message);
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
  }

  g_autoptr(GError) error = NULL;
  g_autoptr(EosAppManager) proxy =
    eos_app_manager_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                            G_DBUS_PROXY_FLAGS_NONE,
                                            "com.endlessm.AppManager",
                                            "/com/endlessm/AppManager",
                                            NULL,
                                            &error);
  if (error != NULL) {
    g_printerr ("Unable to activate eamd: %s\n", error->message);
    return EXIT_FAILURE;
  }

  g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (proxy), G_MAXINT);

  g_autoptr(GVariant) capabilities = NULL;
  eos_app_manager_call_get_user_capabilities_sync (proxy,
                                                   &capabilities,
                                                   NULL,
                                                   &error);

  if (error != NULL) {
    g_printerr ("Unable to retrieve user capabilities: %s", error->message);
    return EXIT_FAILURE;
  }

  gboolean can_uninstall = FALSE;
  g_variant_lookup (capabilities, "CanUninstall", "b", &can_uninstall);
  if (!can_uninstall) {
    g_printerr ("You cannot uninstall applications.\n");
    return EXIT_FAILURE;
  }

  gboolean retval = FALSE;
  eos_app_manager_call_uninstall_sync (proxy, appid, &retval, NULL, &error);
  if (error != NULL) {
    g_printerr ("Unable to uninstall '%s': %s\n", appid, error->message);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
