/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include "eam-os.h"

#include <errno.h>
#include <string.h>

/*
 * Parse the /etc/os-release file if present:
 * http://www.freedesktop.org/software/systemd/man/os-release.html
 */
static gboolean
parse_os_release_file (gchar **version_id, GError **error)
{
  char *contents = NULL;
  char **lines;
  gint idx;

  if (!g_file_get_contents ("/etc/os-release", &contents, NULL, error))
    return FALSE;

  lines = g_strsplit (contents, "\n", -1);

  gboolean ret = FALSE;
  gchar *line = NULL;

  for (idx = 0; lines[idx] != NULL; idx++) {
    line = lines[idx];
    line = g_strstrip (line);

    if (!g_str_has_prefix (line, "VERSION_ID")) {
      continue;
    }

    gchar *p = strchr (line, '=');
    if (!p) {
      continue;
    }

    do
      p++;
    while (g_ascii_isspace (*p) || *p == '"');

    gchar *start = p;

    do
      p++;
    while (*p != '"' && *p != '\0');

    *version_id = g_strndup (start, p - start);
    ret = TRUE;

    break;
  }

  g_free (contents);
  g_strfreev (lines);

  return ret;
}

/**
 * eam_os_get_version:
 *
 * Queries the EndlessOS version.
 *
 * Returns: A string with the EOS version
 **/
const gchar *
eam_os_get_version ()
{
  static gchar *version = NULL;

  if (version)
    return version;

  GError *error = NULL;
  parse_os_release_file (&version, &error);

  if (error) {
    g_critical ("Can't parse /etc/os-release file: %s", error->message);
    g_clear_error (&error);
    goto bail;
  }

bail:
  if (!version)
    return "1.0"; /* is this a sensible fallback? */
  return version;
}

/**
 * eam_os_get_personality:
 *
 * Queries the EndlessOS personality.
 *
 * Returns: A string with the EOS personality
 **/
const gchar *
eam_os_get_personality ()
{
  static gchar *personality = NULL;

  if (personality)
    return personality;

  GError *error = NULL;
  GKeyFile *conffile = g_key_file_new ();

  g_key_file_load_from_file (conffile, "/etc/EndlessOS/personality.conf",
    G_KEY_FILE_NONE, &error);
  if (error) {
    g_critical ("Can't open personality file: %s", error->message);
    g_clear_error (&error);
    goto bail;
  }

  personality = g_key_file_get_string (conffile, "Personality",
    "PersonalityName", &error);
  if (error) {
    g_critical ("Can't get personality field: %s", error->message);
    g_clear_error (&error);
    goto bail;
  }

bail:
  g_key_file_unref (conffile);
  if (!personality)
    return "Global"; /* is this a sensible fallback? */
  return personality;
}

/**
 * eam_os_get_architecture:
 *
 * Queries the EndlessOS architecture.
 *
 * Returns: A string with the EOS architecture.
 **/
const gchar *
eam_os_get_architecture ()
{
  return EOS_ARCH;
}
