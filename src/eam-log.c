/* Copyright 2014 Endless Mobile, Inc. */
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
  static const char *eam_testing;

  if (eam_testing == NULL) {
    const char *env = g_getenv ("EAM_TESTING");
    if (env != NULL && *env == '1')
      eam_testing = "1";
    else
      eam_testing = "0";
  }

  /* if we're not under a test environment, then we send
   * everything to the journal
   */
  if (*eam_testing == '0') {
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

    default:
      vfprintf (stderr, fmt, args);
      fputc ('\n', stderr);
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
