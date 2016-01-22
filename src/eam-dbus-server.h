/* eam-dbus-server.h: DBus server skeleton
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

#pragma once
#include <glib-object.h>

G_BEGIN_DECLS

#define EAM_TYPE_DBUS_SERVER (eam_dbus_server_get_type())

#define EAM_DBUS_SERVER(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EAM_TYPE_DBUS_SERVER, EamDbusServer))

#define EAM_DBUS_SERVER_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST((k), EAM_TYPE_DBUS_SERVER, EamDbusServerClass))

#define EAM_IS_DBUS_SERVER(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAM_TYPE_DBUS_SERVER))

#define EAM_IS_DBUS_SERVER_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EAM_TYPE_DBUS_SERVER))

#define EAM_DBUS_SERVER_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EAM_TYPE_DBUS_SERVER, EamDbusServerClass))

typedef struct _EamDbusServerClass EamDbusServerClass;
typedef struct _EamDbusServer EamDbusServer;

struct _EamDbusServer {
  GObject parent;
};

struct _EamDbusServerClass {
  GObjectClass parent_class;
};

GType          eam_dbus_server_get_type         (void) G_GNUC_CONST;

EamDbusServer *eam_dbus_server_new              (void);

gboolean       eam_dbus_server_run              (EamDbusServer *server);

void           eam_dbus_server_quit             (EamDbusServer *server);

G_END_DECLS
