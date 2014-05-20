/* Copyright 2014 Endless Mobile, Inc. */

#include <glib.h>
#include <string.h>
#include <eam-config.h>

#define config "[eam]\nappdir = /\n" \
  "downloaddir=/tmp\n" \
  "serveraddress = http://nohost\n" \
  "protocolversion = v1000\n" \
  "scriptdir = /usr/libexec/eam\n"

static void
test_config_basic (void)
{
  g_assert_nonnull (eam_config_get ());

  EamConfig *cfg = eam_config_get ();
  g_assert_nonnull (cfg);

  GKeyFile *keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, config, strlen (config), G_KEY_FILE_NONE, NULL);
  g_assert_true (eam_config_load (cfg, keyfile));
  g_key_file_unref (keyfile);

  g_assert_cmpstr (eam_config_appdir (), ==, "/");
  g_assert_cmpstr (eam_config_dldir (), ==, "/tmp");

  eam_config_set (cfg, NULL, g_strdup ("/var/tmp"), NULL, NULL, NULL, 0);
  g_assert_cmpstr (eam_config_appdir (), ==, "/");
  g_assert_cmpstr (eam_config_dldir (), ==, "/var/tmp");
  g_assert_cmpstr (eam_config_saddr (), ==, "http://nohost");
  g_assert_cmpstr (eam_config_protver (), ==, "v1000");
  g_assert_cmpstr (eam_config_scriptdir (), ==, "/usr/libexec/eam");
  g_assert_cmpint (eam_config_timeout (), ==, 0);
}

static void
test_config_singleton (void)
{
  EamConfig *cfg = eam_config_get ();
  g_assert_nonnull (cfg);
  g_assert_cmpstr (eam_config_protver (), ==, "v1000");
  eam_config_free (NULL);

  cfg = eam_config_get ();
  g_assert_null (eam_config_appdir ());

  eam_config_destroy (NULL);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/config/basic", test_config_basic);
  g_test_add_func ("/config/singleton", test_config_singleton);

  return g_test_run ();
}
