/* eam-dbus-utils.h: DBus utility API
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

#include "eam-dbus-utils.h"
#include "eam-log.h"

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
      eam_log_error_message ("Unable to retrieve the UID for '%s': %s",
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
