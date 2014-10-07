/* Copyright 2014 Endless Mobile, Inc. */

#include <glib.h>
#include <string.h>
#include <eam-config.h>
#include <eam-os.h>
#include <eam-rest.h>


#define config "[eam]\nappdir = /endless\n" \
  "downloaddir=/var/tmp\n" \
  "serveraddress = http://localhost\n"

/* @TODO: use a #GTestCase for this fixture */
static void
load_config (const gchar *protver)
{
  EamConfig *cfg = eam_config_get ();
  GKeyFile *keyfile = g_key_file_new ();

  g_key_file_load_from_data (keyfile, config, strlen (config), G_KEY_FILE_NONE, NULL);
  eam_config_load (cfg, keyfile);
  g_key_file_unref (keyfile);

  eam_config_set (cfg, NULL, NULL, NULL, g_strdup (protver), NULL, NULL, 0);
}

static void
test_restv1_basic (void)
{
  gchar *uri, *turi;
  const gchar *version = eam_os_get_version ();
  const gchar *arch = eam_os_get_architecture ();
  const gchar *pers = eam_os_get_personality ();

  load_config ("v1");

  uri = eam_rest_build_uri (EAM_REST_API_V1_GET_ALL_UPDATES, NULL);
  turi = g_strconcat ("http://localhost/api/v1/updates/", version, "?arch=",
                      arch, "&personality=", pers, NULL);
  g_assert_cmpstr (uri, ==, turi);
  g_free (uri);
  g_free (turi);

  uri = eam_rest_build_uri (EAM_REST_API_V1_GET_APP_UPDATES, "com.application.id1", NULL);
  turi = g_strconcat ("http://localhost/api/v1/updates/", version,
                      "/com.application.id1?arch=", arch,
		      "&personality=", pers, NULL);
  g_assert_cmpstr (uri, ==, turi);
  g_free (uri);
  g_free (turi);

  uri = eam_rest_build_uri (EAM_REST_API_V1_GET_APP_UPDATE_LINK, "com.application.id2", NULL);
  turi = g_strconcat ("http://localhost/api/v1/updates/", version,
                      "/com.application.id2?arch=", arch, NULL);
  g_assert_cmpstr (uri, ==, turi);
  g_free (uri);
  g_free (turi);

  uri = eam_rest_build_uri (EAM_REST_API_V1_GET_APP_UPDATE_LINK, "com.application.id2", "2.22", NULL);
  turi = g_strconcat ("http://localhost/api/v1/updates/", version,
                      "/com.application.id2/2.22/", arch, NULL);
  g_assert_cmpstr (uri, ==, turi);
  g_free (uri);
  g_free (turi);

  uri = eam_rest_build_uri (EAM_REST_API_V1_GET_APP_UPDATE_BLOB, "bbccddee-2.22-full", NULL);
  g_assert_cmpstr (uri, ==, "http://localhost/api/v1/updates/blob/bbccddee-2.22-full");
  g_free (uri);
}

static void
test_restv2_basic (void)
{
  load_config ("v2");

  gchar *uri = eam_rest_build_uri (EAM_REST_API_V1_GET_ALL_UPDATES, NULL);
  g_assert_null (uri);
}


int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/restv1/basic", test_restv1_basic);
  g_test_add_func ("/restv2/basic", test_restv2_basic);

  return g_test_run ();
}
