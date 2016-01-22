/* eam-transaction-dbus.h: DBus transaction utility wrapper
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

#ifndef EAM_TRANSACTION_DBUS_H
#define EAM_TRANSACTION_DBUS_H

#include "eam-service.h"
#include "eam-transaction.h"

G_BEGIN_DECLS

typedef struct _EamRemoteTransaction EamRemoteTransaction;

EamRemoteTransaction * eam_remote_transaction_new (EamService *service,
                                                   GDBusConnection *connection,
                                                   const char *sender,
                                                   EamTransaction *transaction,
                                                   GError **error);
const char * eam_remote_transaction_get_obj_path (EamRemoteTransaction *remote);

G_END_DECLS

#endif
