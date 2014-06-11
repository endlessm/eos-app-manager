/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include "eam-dbus-utils.h"

uid_t
eam_dbus_get_uid_for_sender (GDBusConnection *connection,
                             const char *sender)
{
  GError *error = NULL;
  GVariant *value =
    g_dbus_connection_call_sync (connection,
                                 "org.freedesktop.DBus",
                                 "/org/freedesktop/DBus",
                                 "org.freedesktop.DBus", "GetConnectionUnixUser",
                                 g_variant_new ("(s)", sender),
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);
  uid_t res = G_MAXUINT;

  if (error != NULL)
    {
      g_critical ("Unable to retrieve the UID for '%s': %s",
                  sender,
                  error->message);
      g_error_free (error);
      goto out;
    }

  g_variant_get (value, "(u)", &res);

out:
  if (value != NULL)
    g_variant_unref (value);

  return res;
}
