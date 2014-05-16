/* Copyright 2014 Endless Mobile, Inc. */

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
  GIOChannel *file = g_io_channel_new_file ("/etc/os-release", "r", error);
  if (!file)
    return FALSE;

  if (error)
    goto bail;

  gboolean ret = FALSE;
  gchar *line = NULL;
  while (g_io_channel_read_line (file, &line, NULL, NULL, error) == G_IO_STATUS_NORMAL) {
    if (line[0] == '#') {
      g_clear_pointer (&line, g_free);
      continue;
    }

    line = g_strstrip (line);
    gchar *p = strchr (line, '=');
    if (!p) {
      g_clear_pointer (&line, g_free);
      continue;
    }

    if (!g_str_has_prefix (line, "VERSION_ID")) {
      g_clear_pointer (&line, g_free);
      continue;
    }

    do
      p++;
    while (g_ascii_isspace (*p));

    *version_id = g_strdup (p);
    ret = TRUE;

    break;
  }

  g_clear_pointer (&line, g_free);

bail:
  g_io_channel_unref (file);
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
