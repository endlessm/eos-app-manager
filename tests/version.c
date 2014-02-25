/* Copyright 2014 Endless Mobile, Inc. */

#include <glib.h>
#include <eam-version.h>

#define version(epoch, version, revision) \
	(EamPkgVersion) { (epoch), (version), (revision) }

static void
test_version_compare (void)
{
  EamPkgVersion a, b;

  a = version (0, NULL, NULL);
  b = version (0, NULL, NULL);
  g_assert_cmpint (eam_pkg_version_compare (&a, &b), ==, 0);

  a = version (1, NULL, NULL);
  b = version (2, NULL, NULL);
  g_assert_cmpint (eam_pkg_version_compare (&a, &b), <, 0);

  a = version (0, "1", "1");
  b = version (0, "2", "1");
  g_assert_cmpint (eam_pkg_version_compare (&a, &b), <, 0);

  a = version (0, "1", "1");
  b = version (0, "1", "2");
  g_assert_cmpint (eam_pkg_version_compare (&a, &b), <, 0);

  /* Test for version equality. */
  a = b = version (0, "0", "0");
  g_assert_cmpint (eam_pkg_version_compare (&a, &b), ==, 0);

  a = version (0, "0", "00");
  b = version (0, "00", "0");
  g_assert_cmpint (eam_pkg_version_compare(&a, &b), ==, 0);

  a = b = version (1, "2", "3");
  g_assert_cmpint (eam_pkg_version_compare(&a, &b), ==, 0);

  /* Test for epoch difference. */
  a = version (0, "0", "0");
  b = version (1, "0", "0");
  g_assert_cmpint (eam_pkg_version_compare(&a, &b), <, 0);
  g_assert_cmpint (eam_pkg_version_compare(&b, &a), >, 0);

  /* Test for version component difference. */
  a = version (0, "a", "0");
  b = version (0, "b", "0");
  g_assert_cmpint (eam_pkg_version_compare(&a, &b), <, 0);
  g_assert_cmpint (eam_pkg_version_compare(&b, &a), >, 0);

  /* Test for revision component difference. */
  a = version (0, "0", "a");
  b = version (0, "0", "b");
  g_assert_cmpint (eam_pkg_version_compare(&a, &b), <, 0);
  g_assert_cmpint (eam_pkg_version_compare(&b, &a), >, 0);
}

static void
test_version_relate (void)
{
  EamPkgVersion a, b;

  a = b = version (0, NULL, NULL);
  g_assert_true (eam_pkg_version_relate (&a, EAM_RELATION_NONE, &b));

  a = version (0, "1", "1");
  b = version (0, "1", "1");
  g_assert_true (eam_pkg_version_relate (&a, EAM_RELATION_EQ, &b));
  g_assert_false (eam_pkg_version_relate (&a, EAM_RELATION_LT, &b));
  g_assert_true (eam_pkg_version_relate (&a, EAM_RELATION_LE, &b));
  g_assert_false (eam_pkg_version_relate (&a, EAM_RELATION_GT, &b));
  g_assert_true (eam_pkg_version_relate (&a, EAM_RELATION_GE, &b));

  a = version (0, "1", "1");
  b = version (0, "2", "1");
  g_assert_false (eam_pkg_version_relate (&a, EAM_RELATION_EQ, &b));
  g_assert_true (eam_pkg_version_relate (&a, EAM_RELATION_LT, &b));
  g_assert_true (eam_pkg_version_relate (&a, EAM_RELATION_LE, &b));
  g_assert_false (eam_pkg_version_relate (&a, EAM_RELATION_GT, &b));
  g_assert_false (eam_pkg_version_relate (&a, EAM_RELATION_GE, &b));

  a = version (0, "2", "1");
  b = version (0, "1", "1");
  g_assert_false (eam_pkg_version_relate (&a, EAM_RELATION_EQ, &b));
  g_assert_false (eam_pkg_version_relate (&a, EAM_RELATION_LT, &b));
  g_assert_false (eam_pkg_version_relate (&a, EAM_RELATION_LE, &b));
  g_assert_true (eam_pkg_version_relate (&a, EAM_RELATION_GT, &b));
  g_assert_true (eam_pkg_version_relate (&a, EAM_RELATION_GE, &b));
}

static void
test_version_parse (void)
{
  EamPkgVersion *a, b;
  const char *p;
  char *verstr;

  /* Test 0 versions. */
  b = version (0, "0", "");

  a = eam_pkg_version_new_from_string ("0");
  g_assert_nonnull (a);
  g_assert_cmpint (eam_pkg_version_compare (a, &b), ==, 0);
  eam_pkg_version_free (a);

  a = eam_pkg_version_new_from_string ("0:0");
  g_assert_nonnull (a);
  g_assert_cmpint (eam_pkg_version_compare (a, &b), ==, 0);
  eam_pkg_version_free (a);

  a = eam_pkg_version_new_from_string ("0:0-");
  g_assert_nonnull (a);
  g_assert_cmpint (eam_pkg_version_compare (a, &b), ==, 0);
  eam_pkg_version_free (a);

  b = version (0, "0", "0");
  a = eam_pkg_version_new_from_string ("0:0-0");
  g_assert_nonnull (a);
  g_assert_cmpint (eam_pkg_version_compare (a, &b), ==, 0);
  eam_pkg_version_free (a);

  b = version (0, "0.0", "0.0");
  a = eam_pkg_version_new_from_string ("0:0.0-0.0");
  g_assert_nonnull (a);
  g_assert_cmpint (eam_pkg_version_compare (a, &b), ==, 0);
  eam_pkg_version_free (a);

  /* Test epoched versions. */
  b = version (1, "0", "");
  a = eam_pkg_version_new_from_string ("1:0");
  g_assert_nonnull (a);
  g_assert_cmpint (eam_pkg_version_compare (a, &b), ==, 0);
  eam_pkg_version_free (a);

  b = version (5, "1", "");
  a = eam_pkg_version_new_from_string ("5:1");
  g_assert_nonnull (a);
  g_assert_cmpint (eam_pkg_version_compare (a, &b), ==, 0);
  eam_pkg_version_free (a);

  /* Test multiple hyphens. */
  b = version (0, "0-0", "0");
  a = eam_pkg_version_new_from_string ("0:0-0-0");
  g_assert_nonnull (a);
  g_assert_cmpint (eam_pkg_version_compare (a, &b), ==, 0);
  eam_pkg_version_free (a);

  b = version (0, "0-0-0", "0");
  a = eam_pkg_version_new_from_string ("0:0-0-0-0");
  g_assert_nonnull (a);
  g_assert_cmpint (eam_pkg_version_compare (a, &b), ==, 0);
  eam_pkg_version_free (a);

  /* Test multiple colons. */
  b = version (0, "0:0", "0");
  a = eam_pkg_version_new_from_string ("0:0:0-0");
  g_assert_nonnull (a);
  g_assert_cmpint (eam_pkg_version_compare (a, &b), ==, 0);
  eam_pkg_version_free (a);

  b = version (0, "0:0:0", "0");
  a = eam_pkg_version_new_from_string ("0:0:0:0-0");
  g_assert_nonnull (a);
  g_assert_cmpint (eam_pkg_version_compare (a, &b), ==, 0);
  eam_pkg_version_free (a);

  /* Test multiple hyphens and colons. */
  b = version (0, "0:0-0", "0");
  a = eam_pkg_version_new_from_string ("0:0:0-0-0");
  g_assert_nonnull (a);
  g_assert_cmpint (eam_pkg_version_compare (a, &b), ==, 0);
  eam_pkg_version_free (a);

  b = version (0, "0-0:0", "0");
  a = eam_pkg_version_new_from_string ("0:0-0:0-0");
  g_assert_nonnull (a);
  g_assert_cmpint (eam_pkg_version_compare (a, &b), ==, 0);
  eam_pkg_version_free (a);

  /* Test valid characters in upstream version. */
  b = version (0, "09azAZ.-+~:", "0");
  a = eam_pkg_version_new_from_string ("0:09azAZ.-+~:-0");
  g_assert_nonnull (a);
  g_assert_cmpint (eam_pkg_version_compare (a, &b), ==, 0);
  eam_pkg_version_free (a);

  /* Test valid characters in revision. */
  b = version (0, "0", "azAZ09.+~");
  a = eam_pkg_version_new_from_string ("0:0-azAZ09.+~");
  g_assert_nonnull (a);
  g_assert_cmpint (eam_pkg_version_compare (a, &b), ==, 0);
  eam_pkg_version_free (a);

  /* Test empty version. */
  a = eam_pkg_version_new_from_string ("");
  g_assert_null (a);

  /* Test empty upstream version after epoch. */
  a = eam_pkg_version_new_from_string ("0:");
  g_assert_null (a);

  /* Test version from embedded spaces. */
  a = eam_pkg_version_new_from_string ("0:0 0-1");
  g_assert_null (a);

  /* Test invalid characters in epoch. */
  a = eam_pkg_version_new_from_string ("a:0-0");
  g_assert_null (a);
  a = eam_pkg_version_new_from_string ("A:0-0");
  g_assert_null (a);

  /* Test upstream version not starting from a digit */
  a = eam_pkg_version_new_from_string ("0:abc3-0");
  g_assert_null (a);

  /* Test invalid characters in upstream version. */
  verstr = g_strdup ("0:0a-0");
  for (p = "!#@$%&/|\\<>()[]{};,_=*^'"; *p; p++) {
    verstr[3] = *p;
    a = eam_pkg_version_new_from_string (verstr);
    g_assert_null (a);
  }
  g_free (verstr);

  /* Test invalid characters in revision. */
  a = eam_pkg_version_new_from_string ("0:0-0:0");
  g_assert_null (a);

  verstr = g_strdup ("0:0-0");
  for (p = "!#@$%&/|\\<>()[]{}:;,_=*^'"; *p; p++) {
    verstr[4] = *p;
    a = eam_pkg_version_new_from_string (verstr);
    g_assert_null (a);
  }
  g_free (verstr);

  /* @FIXME: Complete. */
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/version/compare", test_version_compare);
  g_test_add_func ("/version/relate", test_version_relate);
  g_test_add_func ("/version/parse", test_version_parse);

  return g_test_run ();
}
