/* eam-transaction.h: Base transaction class
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

#ifndef EAM_TRANSACTION_H
#define EAM_TRANSACTION_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define EAM_TYPE_TRANSACTION (eam_transaction_get_type ())

#define EAM_TRANSACTION(o) \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), EAM_TYPE_TRANSACTION, EamTransaction))

#define EAM_IS_TRANSACTION(o) \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAM_TYPE_TRANSACTION))

#define EAM_TRANSACTION_GET_IFACE(o) \
        (G_TYPE_INSTANCE_GET_INTERFACE ((o), EAM_TYPE_TRANSACTION, EamTransactionInterface))

#define EAM_TYPE_IS_TRANSACTIONABLE(type) \
        (g_type_is_a ((type), EAM_TYPE_TRANSACTION))


/**
 * EamTransaction:
 *
 * Interface for asynchronous transaction objects.
 **/
typedef struct _EamTransaction EamTransaction;
typedef struct _EamTransactionInterface EamTransactionInterface;

/**
 * EamTransactionInterface:
 * @g_iface: The parent interface.
 * @run: Starts the transaction.
 * @finish: Finish the transaction.
 **/
struct _EamTransactionInterface
{
  GTypeInterface g_iface;

  gboolean       (* run_sync)   (EamTransaction *trans,
                                 GCancellable *cancellable,
                                 GError **error);
  void           (* run_async)  (EamTransaction *trans,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer data);

  gboolean       (* finish)     (EamTransaction *trans,
                                 GAsyncResult *res,
                                 GError **error);
};

GType            eam_transaction_get_type (void) G_GNUC_CONST;

gboolean         eam_transaction_run_sync       (EamTransaction *trans,
                                                 GCancellable *cancellable,
                                                 GError **error);
void             eam_transaction_run_async      (EamTransaction *trans,
                                                 GCancellable *cancellable,
                                                 GAsyncReadyCallback callback,
                                                 gpointer data);

gboolean         eam_transaction_finish         (EamTransaction *trans,
                                                 GAsyncResult *res,
                                                 GError **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (EamTransaction, g_object_unref)

G_END_DECLS

#endif /* EAM_TRANSACTION_H */
