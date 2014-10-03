/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_LOG_H
#define EAM_LOG_H

#include <glib.h>

G_BEGIN_DECLS

void    eam_log_debug_message   (const char *fmt,
                                 ...) G_GNUC_PRINTF (1, 2);
void    eam_log_error_message   (const char *fmt,
                                 ...) G_GNUC_PRINTF (1, 2);
void    eam_log_info_message    (const char *fmt,
                                 ...) G_GNUC_PRINTF (1, 2);

G_END_DECLS

#endif /* EAM_OS_H */
