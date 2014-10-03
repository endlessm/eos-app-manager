/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include "eam-log.h"

#include <errno.h>
#include <string.h>
#include <systemd/sd-journal.h>

void
eam_log_debug_message (const char *fmt,
                       ...)
{
  va_list args;

  va_start (args, fmt);

  sd_journal_printv (LOG_DEBUG, fmt, args);

  va_end (args);
}

void
eam_log_error_message (const char *fmt,
                       ...)
{
  va_list args;

  va_start (args, fmt);

  sd_journal_printv (LOG_ERR, fmt, args);

  va_end (args);
}

void
eam_log_info_message (const char *fmt,
                      ...)
{
  va_list args;

  va_start (args, fmt);

  sd_journal_printv (LOG_INFO, fmt, args);

  va_end (args);
}
