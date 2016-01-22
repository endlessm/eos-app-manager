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
#include "eam-update.h"
#include "eam-utils.h"

#include "eos-app-manager-service.h"
#include "eos-app-manager-transaction.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>

#include <glib.h>
#include <gio/gio.h>

static char *opt_prefix;
static char *opt_asc_file;
static char **opt_appid;
static char *opt_source_storage_type;
static char *opt_target_storage_type;

static const GOptionEntry install_entries[] = {
 { "prefix", 0, 0, G_OPTION_ARG_FILENAME, &opt_prefix, "Prefix to use when updating", "DIRECTORY" },
 { "signature-file", 's', 0, G_OPTION_ARG_FILENAME, &opt_asc_file, "Path to the ASC file", "FILE" },
 { "source-storage-type", 'S', 0, G_OPTION_ARG_STRING, &opt_source_storage_type, "Source storage type ('primary' or 'secondary')", "TYPE" },
 { "target-storage-type", 'S', 0, G_OPTION_ARG_STRING, &opt_source_storage_type, "Target storage type ('primary' or 'secondary')", "TYPE" },
 { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &opt_appid, "Application id", "APPID" },
 { NULL },
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC (EosAppManager, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EosAppManagerTransaction, g_object_unref)

int
eam_command_update (int argc, char *argv[])
{
  g_autoptr(GOptionContext) context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, install_entries, GETTEXT_PACKAGE);

  if (!g_option_context_parse (context, &argc, &argv, NULL)) {
    g_printerr ("Usage: %s update [-s SIGNATURE] [-c CHECKSUM] [-d] APPID BUNDLE\n", eam_argv0);
    return EXIT_FAILURE;
  }

  if (opt_appid == NULL || g_strv_length (opt_appid) > 2) {
    g_printerr ("Usage: %s update [-s SIGNATURE] [-c CHECKSUM] [-d] APPID BUNDLE\n", eam_argv0);
    return EXIT_FAILURE;
  }

  if (opt_source_storage_type != NULL &&
      !(strcmp (opt_source_storage_type, "primary") == 0 ||
        strcmp (opt_source_storage_type, "secondary") == 0)) {
    g_printerr ("Invalid source storage type '%s'; you can only use either 'primary' "
                "or 'secondary'\n",
                opt_source_storage_type);
    return EXIT_FAILURE;
  }

  if (opt_target_storage_type != NULL &&
      !(strcmp (opt_target_storage_type, "primary") == 0 ||
        strcmp (opt_target_storage_type, "secondary") == 0)) {
    g_printerr ("Invalid target storage type '%s'; you can only use either 'primary' "
                "or 'secondary'\n",
                opt_target_storage_type);
    return EXIT_FAILURE;
  }

  const char *appid = opt_appid[0];
  if (appid == NULL) {
    g_printerr ("Usage: %s update [-S SIGNATURE] [-c CHECKSUM] [-d] APPID BUNDLE\n", eam_argv0);
    return EXIT_FAILURE;
  }

  g_autofree char *bundle_file = g_strdup (opt_appid[1]);
  if (bundle_file == NULL) {
    g_autofree char *filename = g_strconcat (appid, ".bundle", NULL);
    bundle_file = g_build_filename (g_get_current_dir (), filename, NULL);

    if (!g_file_test (bundle_file, G_FILE_TEST_EXISTS)) {
      g_autofree char *delta_filename = g_strconcat (appid, ".xdelta", NULL);
      g_free (bundle_file);
      bundle_file = g_build_filename (g_get_current_dir (), delta_filename, NULL);
    }

    if (!g_file_test (bundle_file, G_FILE_TEST_EXISTS)) {
      g_printerr ("No bundle file found in the current directory for app '%s'\n."
                  "Specify the bundle file path.\n"
                  "  Usage: %s update APPID BUNDLEFILE\n", appid, eam_argv0);
      return EXIT_FAILURE;
    }
  }

  if (opt_asc_file == NULL) {
    g_autofree char *dirname = g_path_get_dirname (bundle_file);
    g_autofree char *filename = g_strconcat (appid, ".asc", NULL);
    opt_asc_file = g_build_filename (dirname, filename, NULL);

    if (!g_file_test (opt_asc_file, G_FILE_TEST_EXISTS)) {
      g_printerr ("No signature file found. Use --signature-file to specify the signature file.\n");
      return EXIT_FAILURE;
    }
  }

  /* If we are being called by a privileged user, then we bypass the
   * daemon entirely, because we have enough privileges.
   */
  if (eam_utils_can_touch_applications_dir (geteuid ())) {
    g_autoptr(EamUpdate) update = (EamUpdate *) eam_update_new (appid);

    /* An explicit prefix takes precedence */
    if (opt_prefix != NULL) {
      eam_update_set_source_prefix (update, opt_prefix);
    }
    else if (opt_source_storage_type != NULL) {
      if (strcmp (opt_source_storage_type, "primary") == 0)
        eam_update_set_source_prefix (update, eam_config_get_primary_storage ());
      else if (strcmp (opt_source_storage_type, "secondary") == 0)
        eam_update_set_source_prefix (update, eam_config_get_secondary_storage ());
      else
        g_assert_not_reached ();
    }

    if (opt_target_storage_type != NULL) {
      if (strcmp (opt_target_storage_type, "primary") == 0)
        eam_update_set_target_prefix (update, eam_config_get_primary_storage ());
      else if (strcmp (opt_target_storage_type, "secondary") == 0)
        eam_update_set_target_prefix (update, eam_config_get_secondary_storage ());
      else
        g_assert_not_reached ();
    }

    eam_update_set_bundle_file (update, bundle_file);
    eam_update_set_signature_file (update, opt_asc_file);

    g_autoptr(GError) error = NULL;
    eam_transaction_run_sync (EAM_TRANSACTION (update), NULL, &error);
    if (error != NULL) {
      g_printerr ("Update failed: %s\n", error->message);
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
  }

  /* Fall back to call the DBus API on the app manager */

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
    g_printerr ("Unable to retrieve user capabilities: %s",
                error->message);
    return EXIT_FAILURE;
  }

  gboolean can_update = FALSE;
  g_variant_lookup (capabilities, "CanUpdate", "b", &can_update);
  if (!can_update) {
    g_printerr ("You cannot update applications.\n");
    return EXIT_FAILURE;
  }

  g_autofree char *transaction_path = NULL;
  eos_app_manager_call_update_sync (proxy, appid,
                                    &transaction_path,
                                    NULL,
                                    &error);
  if (error != NULL) {
    g_printerr ("Unable to begin the update transaction for '%s': %s\n",
                appid, error->message);
    return EXIT_FAILURE;
  }

  g_autoptr(EosAppManagerTransaction) transaction =
    eos_app_manager_transaction_proxy_new_sync (g_dbus_proxy_get_connection (G_DBUS_PROXY (proxy)),
                                                G_DBUS_PROXY_FLAGS_NONE,
                                                "com.endlessm.AppManager",
                                                transaction_path,
                                                NULL,
                                                &error);
  if (error != NULL) {
    g_printerr ("Unable to handle the transaction: %s\n", error->message);
    return EXIT_FAILURE;
  }

  g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (transaction), G_MAXINT);

  GVariantBuilder opts;
  g_variant_builder_init (&opts, G_VARIANT_TYPE ("a{sv}"));
  if (opt_source_storage_type)
    g_variant_builder_add (&opts, "{sv}", "SourceStorageType", g_variant_new_string (opt_source_storage_type));
  if (opt_target_storage_type)
    g_variant_builder_add (&opts, "{sv}", "TargetStorageType", g_variant_new_string (opt_target_storage_type));
  g_variant_builder_add (&opts, "{sv}", "BundlePath", g_variant_new_string (bundle_file));
  g_variant_builder_add (&opts, "{sv}", "SignaturePath", g_variant_new_string (opt_asc_file));

  gboolean retval = FALSE;
  eos_app_manager_transaction_call_complete_transaction_sync (transaction,
                                                              g_variant_builder_end (&opts),
                                                              &retval,
                                                              NULL,
                                                              &error);
  if (error != NULL) {
    g_printerr ("Unable to complete the transaction: %s\n", error->message);

    /* Cancel the transaction */
    eos_app_manager_transaction_call_cancel_transaction_sync (transaction, NULL, NULL);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
