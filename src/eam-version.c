/* Copyright 2014 Endless Mobile, Inc. */

/* Based on dpkg:
 *
 * Copyright © 1995 Ian Jackson <ian@chiark.greenend.org.uk>
 * Copyright © 2006-2012 Guillem Jover <guillem@debian.org>
 */

#include <config.h>
#include <string.h>
#include "eam-version.h"

G_DEFINE_BOXED_TYPE (EamPkgVersion, eam_pkg_version_new, eam_pkg_version_copy, eam_pkg_version_free)

/**
 * eam_pkg_version_new:
 *
 * Allocates a new #EamPkgVersion with the parsed string.
 *
 * Returns: a new #EamPkgVersion
 */
EamPkgVersion *
eam_pkg_version_new ()
{
  return g_new0 (EamPkgVersion, 1);
}

/**
 * eam_pkg_version_new_from_string:
 * @string: the version string to parse
 *
 * Allocates a new #EamPkgVersion with the parsed string.
 *
 * Returns: a new #EamPkgVersion or %NULL if the string can't be
 * parsed,
 */
EamPkgVersion *
eam_pkg_version_new_from_string (const gchar *string)
{
  EamPkgVersion *version = eam_pkg_version_new ();
  if (!eam_pkg_version_parse (version, string)) {
    eam_pkg_version_free (version);
    return NULL;
  }
  return version;
}

/**
 * eam_pkg_version_copy:
 * @version: the version string to copy
 *
 * Allocates a new #EamPkgVersion and copy the values of @vesion
 *
 * Returns: a new #EamPkgVersion
 */
EamPkgVersion *
eam_pkg_version_copy (const EamPkgVersion *version)
{
  EamPkgVersion *new = eam_pkg_version_new ();
  new->epoch = version->epoch;
  new->version = g_strdup (version->version);
  new->revision = g_strdup (version->revision);

  return new;
}

/**
 * eam_pkg_version_free:
 * @version: an allocated #EamPkgVersion structure
 *
 * Frees an allocated #EamPkgVersion.
 */
void
eam_pkg_version_free (EamPkgVersion *version)
{
  g_return_if_fail (version);

  g_free (version->version);
  g_free (version->revision);
  g_free (version);
}

/**
 * eam_pkg_version_parse:
 * @version The parsed version.
 * @string The version string to parse.
 *
 * Parse a version string and check for invalid syntax.
 *
 * Returns: %TRUE On success, %FALSE otherwise.
 */
gboolean
eam_pkg_version_parse (EamPkgVersion *version, const gchar *string)
{
  const gchar *end, *ptr;
  g_return_val_if_fail (version, FALSE);

  if (!*string)
    return FALSE;

  if (!g_str_is_ascii (string))
    return FALSE;

  /* Trim leading and trailing space. */
  while (*string && g_ascii_isspace (*string))
    string++;
  /* String now points to the first non-whitespace char. */
  end = string;
  /* Find either the end of the string, or a whitespace char. */
  while (*end && !g_ascii_isspace (*end))
    end++;
  /* Check for extra chars after trailing space. */
  ptr = end;
  while (*ptr && g_ascii_isspace (*ptr))
    ptr++;
  if (*ptr)
    return FALSE; /* version string has embedded spaces */

  gchar *colon = strchr (string, ':');
  if (colon) {
    gchar *eepochcolon;
    gdouble epoch = g_strtod (string, &eepochcolon);
    if (colon != eepochcolon)
      return FALSE; /* epoch is not a number */
    if (epoch < 0)
      return FALSE; /* epoch is negative */
    if (epoch > G_MAXINT)
      return FALSE; /* epoch is too big */
    if (!*++colon)
      return FALSE; /* nothing after colon in version number */
    string = colon;
    version->epoch = epoch;
  } else {
    version->epoch = 0;
  }

  version->version = g_strndup (string, end - string);
  gchar *hyphen = strrchr (version->version, '-');
  if (hyphen)
    *hyphen++ = '\0';
  version->revision = hyphen ? g_strdup (hyphen) : NULL;

  ptr = version->version;
  if (ptr && *ptr && !g_ascii_isdigit(*ptr++))
    return FALSE; /* version number doesn't start with digit */
  for (; ptr && *ptr; ptr++) {
    if (!g_ascii_isdigit(*ptr) && !g_ascii_isalpha(*ptr) && strchr(".-+~:", *ptr) == NULL)
      return FALSE; /* invalid character in version number */
  }

  for (ptr = version->revision; ptr && *ptr; ptr++) {
    if (!g_ascii_isdigit(*ptr) && !g_ascii_isalpha(*ptr) && strchr(".+~", *ptr) == NULL)
      return FALSE; /* invalid characters in revision number */
  }

  return TRUE;
}

static gint
order (gint c)
{
  if (g_ascii_isdigit (c))
    return 0;
  else if (g_ascii_isalpha (c))
    return c;
  else if (c == '~')
    return -1;
  else if (c)
    return c + 256;
  else
    return 0;
}

static gint
verrevcmp (const char *a, const char *b)
{
  if (a == NULL)
    a = "";
  if (b == NULL)
    b = "";

  while (*a || *b) {
    int first_diff = 0;

    while ((*a && !g_ascii_isdigit (*a)) || (*b && !g_ascii_isdigit (*b))) {
      int ac = order (*a);
      int bc = order (*b);

      if (ac != bc)
        return ac - bc;

      a++;
      b++;
    }

    while (*a == '0')
      a++;

    while (*b == '0')
      b++;

    while (g_ascii_isdigit (*a) && g_ascii_isdigit (*b)) {
      if (!first_diff)
        first_diff = *a - *b;
      a++;
      b++;
    }

    if (g_ascii_isdigit (*a))
      return 1;
    if (g_ascii_isdigit (*b))
      return -1;
    if (first_diff)
      return first_diff;
  }

  return 0;
}

/**
 * eam_pkg_version_compare:
 * @a: The first version.
 * @b: The second version.
 *
 * Compares two package versions.
 *
 * Returns: 0 if both are equal, <0 if a is smaller than b, >0 if a is
 * greater than b.
 */
gint
eam_pkg_version_compare (const EamPkgVersion *a, const EamPkgVersion *b)
{
	int rc;

	if (a->epoch > b->epoch)
		return 1;
	if (a->epoch < b->epoch)
		return -1;

	rc = verrevcmp (a->version, b->version);
	if (rc)
		return rc;

	return verrevcmp (a->revision, b->revision);
}

/**
 * eam_pkg_version_relate:
 * @a: The first version.
 * @rel: The relation.
 * @b: The second version.
 *
 * Check if two versions have a certain relation.
 *
 * Returns: %TRUE If the expression “a rel b” is true, of if rel is
 * #EAM_RELATION_NONE. %FALSE otherwise.
 */
gboolean
eam_pkg_version_relate (const EamPkgVersion *a, EamRelation rel, const EamPkgVersion *b)
{
  if (rel == EAM_RELATION_NONE)
    return TRUE;

  int rc = eam_pkg_version_compare (a, b);

  switch (rel) {
  case EAM_RELATION_EQ:
    return rc == 0;
  case EAM_RELATION_LT:
    return rc < 0;
  case EAM_RELATION_LE:
    return rc <= 0;
  case EAM_RELATION_GT:
    return rc > 0;
  case EAM_RELATION_GE:
    return rc >= 0;
  default:
    g_assert_not_reached ();
  }

  return FALSE;
}
