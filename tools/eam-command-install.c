#include "config.h"

#include "eam-commands.h"

#include "eam-config.h"
#include "eam-error.h"
#include "eam-fs-sanity.h"
#include "eam-install.h"
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
static char *opt_sha_file;
static char **opt_appid;

static const GOptionEntry install_entries[] = {
 { "prefix", 0, 0, G_OPTION_ARG_FILENAME, &opt_prefix, "Prefix to use when installing", "DIRECTORY" },
 { "signature-file", 's', 0, G_OPTION_ARG_FILENAME, &opt_asc_file, "Path to the ASC file", "FILE" },
 { "checksum-file", 'c', 0, G_OPTION_ARG_FILENAME, &opt_sha_file, "Path to the SHA file", "FILE" },
 { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &opt_appid, "Application id", "APPID" },
 { NULL },
};

int
eam_command_install (int argc, char *argv[])
{
  g_autoptr(GOptionContext) context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, install_entries, GETTEXT_PACKAGE);

  if (!g_option_context_parse (context, &argc, &argv, NULL)) {
    g_printerr ("Usage: %s install [-s SIGNATURE] [-c CHECKSUM] APPID BUNDLE\n", eam_argv0);
    return EXIT_FAILURE;
  }

  if (opt_appid == NULL || g_strv_length (opt_appid) > 2) {
    g_printerr ("Usage: %s install [-s SIGNATURE] [-c CHECKSUM] APPID BUNDLE\n", eam_argv0);
    return EXIT_FAILURE;
  }

  const char *appid = opt_appid[0];
  if (appid == NULL) {
    g_printerr ("Usage: %s install [-s SIGNATURE] [-c CHECKSUM] APPID BUNDLE\n", eam_argv0);
    return EXIT_FAILURE;
  }

  const char *bundle_file = opt_appid[1];
  if (bundle_file == NULL) {
    g_autofree char *filename = g_strconcat (appid, ".bundle", NULL);
    bundle_file = g_build_filename (g_get_current_dir (), filename, NULL);

    if (!g_file_test (bundle_file, G_FILE_TEST_EXISTS)) {
      g_printerr ("No bundle file found in the current directory for app '%s'.\n"
                  "Specify the bundle file path.\n"
                  "  Usage: %s install APPID BUNDLEFILE\n", appid, eam_argv0);
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

  if (opt_sha_file == NULL) {
    g_autofree char *dirname = g_path_get_dirname (bundle_file);
    g_autofree char *filename = g_strconcat (appid, ".sha256", NULL);
    opt_sha_file = g_build_filename (dirname, filename, NULL);

    if (!g_file_test (opt_sha_file, G_FILE_TEST_EXISTS)) {
      g_printerr ("No checksum file found. Use --checksum-file to specify the checksum file.\n");
      return EXIT_FAILURE;
    }
  }

  /* If we are being called by a privileged user, then we bypass the
   * daemon entirely, because we have enough privileges.
   */
  if (eam_utils_check_unix_permissions (geteuid ())) {
    g_autoptr(EamInstall) install = (EamInstall *) eam_install_new (appid);

    /* If opt_prefix is unset, eam_config_appdir() is going to be the default */
    eam_install_set_prefix (install, opt_prefix);

    eam_install_set_bundle_file (install, bundle_file);
    eam_install_set_signature_file (install, opt_asc_file);
    eam_install_set_checksum_file (install, opt_sha_file);

    g_autoptr(GError) error = NULL;
    eam_transaction_run_sync (EAM_TRANSACTION (install), NULL, &error);
    if (error != NULL) {
      g_printerr ("Installation failed: %s\n", error->message);
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
  }

  g_autoptr(GError) error = NULL;
  EosAppManager *proxy =
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
    g_object_unref (proxy);
    return EXIT_FAILURE;
  }

  gboolean can_install = FALSE;
  g_variant_lookup (capabilities, "CanInstall", "b", &can_install);
  if (!can_install) {
    g_printerr ("You cannot install applications.\n");
    g_object_unref (proxy);
    return EXIT_FAILURE;
  }

  g_autofree char *transaction_path = NULL;
  eos_app_manager_call_install_sync (proxy, appid,
                                     &transaction_path,
                                     NULL,
                                     &error);
  if (error != NULL) {
    g_printerr ("Unable to begin the installation transaction for '%s': %s\n",
                appid, error->message);
    g_object_unref (proxy);
    return EXIT_FAILURE;
  }

  EosAppManagerTransaction *transaction =
    eos_app_manager_transaction_proxy_new_sync (g_dbus_proxy_get_connection (G_DBUS_PROXY (proxy)),
                                                G_DBUS_PROXY_FLAGS_NONE,
                                                "com.endlessm.AppManager",
                                                transaction_path,
                                                NULL,
                                                &error);
  if (error != NULL) {
    g_printerr ("Unable to handle the transaction: %s\n", error->message);
    g_object_unref (proxy);
    return EXIT_FAILURE;
  }

  g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (transaction), G_MAXINT);

  GVariantBuilder opts;
  g_variant_builder_init (&opts, G_VARIANT_TYPE ("(a{sv})"));
  g_variant_builder_open (&opts, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&opts, "{sv}", "BundlePath", g_variant_new_string (bundle_file));
  g_variant_builder_add (&opts, "{sv}", "SignaturePath", g_variant_new_string (opt_asc_file));
  g_variant_builder_add (&opts, "{sv}", "ChecksumPath", g_variant_new_string (opt_sha_file));
  g_variant_builder_close (&opts);

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

    g_object_unref (transaction);
    g_object_unref (proxy);
    return EXIT_FAILURE;
  }

  g_object_unref (transaction);
  g_object_unref (proxy);

  return EXIT_SUCCESS;
}
