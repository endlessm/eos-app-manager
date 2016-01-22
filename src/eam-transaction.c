/* eam-transaction.c: Base transaction class
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

#include "eam-transaction.h"
#include "eam-error.h"

G_DEFINE_INTERFACE (EamTransaction, eam_transaction, G_TYPE_OBJECT)

static void
eam_transaction_default_init (EamTransactionInterface *iface)
{
}

gboolean
eam_transaction_run_sync (EamTransaction *trans,
                          GCancellable *cancellable,
                          GError **error)
{
  g_return_val_if_fail (EAM_IS_TRANSACTION (trans), FALSE);

  return EAM_TRANSACTION_GET_IFACE (trans)->run_sync (trans, cancellable, error);
}

/**
 * eam_transaction_run_async:
 * @trans: a #GType supporting #EamTransaction.
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the resquest is satisfied.
 * @data: (closure): the data to pass to callback function.
 *
 * Run the main method of the transaction.
 **/
void
eam_transaction_run_async (EamTransaction *trans,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer data)
{
  g_return_if_fail (EAM_IS_TRANSACTION (trans));

  EAM_TRANSACTION_GET_IFACE (trans)->run_async (trans, cancellable, callback, data);
}

/**
 * eam_transaction_finish:
 * @trans: a #GType supporting #EamTransaction.
 * @res: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Finishes the transaction.
 *
 * Returns: %TRUE if everything went OK.
 **/
gboolean
eam_transaction_finish (EamTransaction *trans,
                        GAsyncResult *res,
                        GError **error)
{
  g_return_val_if_fail (EAM_IS_TRANSACTION (trans), FALSE);

  return EAM_TRANSACTION_GET_IFACE (trans)->finish (trans, res, error);
}
