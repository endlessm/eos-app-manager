/* eam-log.c: Logging API
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

#include "eam-log.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <systemd/sd-journal.h>

static void
eam_log_messagev (int level,
                  const char *fmt,
                  va_list args)
{
  static char eam_testing = '\0';
  static char eam_journal_debugging = '\0';

  if (eam_testing == '\0') {
    const char *env = g_getenv ("EAM_TESTING");
    if (env != NULL && *env == '1')
      eam_testing = '1';
    else
      eam_testing = '0';
  }

  if (eam_journal_debugging == '\0') {
    const char *env = g_getenv ("EAM_DEBUG_JOURNAL");
    if (env != NULL && *env == '1')
      eam_journal_debugging = '1';
    else
      eam_journal_debugging = '0';
  }

  if (eam_journal_debugging == '1') {
    sd_journal_printv (level, fmt, args);
    return;
  }

  switch (level) {
    case LOG_DEBUG:
      g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, fmt, args);
      return;

    case LOG_INFO:
      g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, fmt, args);
      return;

    case LOG_ERR:
      /* if we're not under a test environment, then we send
       * errors to the journal
       */
      if (eam_testing == '0') {
        sd_journal_printv (level, fmt, args);
      } else {
        vfprintf (stderr, fmt, args);
        fputc ('\n', stderr);
      }
      return;

    default:
      break;
  }
}

void
eam_log_debug_message (const char *fmt,
                       ...)
{
  va_list args;

  va_start (args, fmt);

  eam_log_messagev (LOG_DEBUG, fmt, args);

  va_end (args);
}

void
eam_log_error_message (const char *fmt,
                       ...)
{
  va_list args;

  va_start (args, fmt);

  eam_log_messagev (LOG_ERR, fmt, args);

  va_end (args);
}

void
eam_log_info_message (const char *fmt,
                      ...)
{
  va_list args;

  va_start (args, fmt);

  eam_log_messagev (LOG_INFO, fmt, args);

  va_end (args);
}
