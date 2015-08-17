/* Copyright 2014 Endless Mobile, Inc. */

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
