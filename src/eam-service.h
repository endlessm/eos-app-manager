/* eam-server.h: DBus server implementation
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
#include <gio/gio.h>

#include "eam-service-generated.h"

G_BEGIN_DECLS

#define EAM_TYPE_SERVICE (eam_service_get_type())

#define EAM_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EAM_TYPE_SERVICE, EamService))

#define EAM_IS_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EAM_TYPE_SERVICE))

#define EAM_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), EAM_TYPE_SERVICE, EamServiceClass))

#define EAM_IS_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), EAM_TYPE_SERVICE))

#define EAM_SERVICE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EAM_TYPE_SERVICE, EamServiceClass))

typedef struct _EamService        EamService;
typedef struct _EamServiceClass   EamServiceClass;

struct _EamService {
  EamAppManagerSkeleton parent;
};

struct _EamServiceClass {
  EamAppManagerSkeletonClass parent_class;
};

GType           eam_service_get_type                              (void) G_GNUC_CONST;

EamService     *eam_service_new                                   (void);

void            eam_service_dbus_register                         (EamService *service,
                                                                   GDBusConnection *connection);

guint           eam_service_get_idle                              (EamService *service);

gboolean        eam_service_load_authority                        (EamService *service,
                                                                   GError **error);

void            eam_service_pop_busy                              (EamService *service);
void            eam_service_reset_timer                           (EamService *service);
char *          eam_service_get_next_transaction_path             (EamService *service);

G_END_DECLS
