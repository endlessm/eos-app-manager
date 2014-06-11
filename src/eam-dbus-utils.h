/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_DBUS_UTILS_H
#define EAM_DBUS_UTILS_H

#include <gio/gio.h>
#include <sys/types.h>

G_BEGIN_DECLS

uid_t   eam_dbus_get_uid_for_sender     (GDBusConnection *connection,
                                         const char      *sender);

G_END_DECLS

#endif
