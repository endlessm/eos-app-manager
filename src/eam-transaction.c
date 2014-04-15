/* Copyright 2014 Endless Mobile, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "eam-transaction.h"

G_DEFINE_INTERFACE (EamTransaction, eam_transaction, G_TYPE_OBJECT)

static void
eam_transaction_default_init (EamTransactionInterface *iface)
{
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
eam_transaction_run_async (EamTransaction *trans, GCancellable *cancellable,
  GAsyncReadyCallback callback, gpointer data)
{
  g_return_if_fail (EAM_IS_TRANSACTION (trans));

  EamTransactionInterface *iface = EAM_TRANSACTION_GET_IFACE (trans);
  (* iface->run_async) (trans, cancellable, callback, data);
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
eam_transaction_finish (EamTransaction *trans, GAsyncResult *res, GError **error)
{
  g_return_val_if_fail (EAM_IS_TRANSACTION (trans), FALSE);

  EamTransactionInterface *iface = EAM_TRANSACTION_GET_IFACE (trans);
  return (* iface->finish) (trans, res, error);
}
