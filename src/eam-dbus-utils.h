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

#ifndef EAM_DBUS_UTILS_H
#define EAM_DBUS_UTILS_H

#include <gio/gio.h>
#include <sys/types.h>

G_BEGIN_DECLS

uid_t   eam_dbus_get_uid_for_sender     (GDBusConnection *connection,
                                         const char      *sender);

G_END_DECLS

#endif
