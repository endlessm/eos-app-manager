/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_TRANSACTION_H
#define EAM_TRANSACTION_H

#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * EamTransactionError:
 * @EAM_TRANSACTION_ERROR_PROTOCOL_ERROR: Invalid URI.
 * @EAM_TRANSACTION_ERROR_INVALID_FILE: Invalid JSON file.
 * @EAM_TRANSACTION_ERROR_NO_NETWORK: The network is not available.
 *
 * These constants identify all the available errors managed by
 * the Endless Application Manager transactions.
 */
typedef enum {
  EAM_TRANSACTION_ERROR_PROTOCOL_ERROR = 1,
  EAM_TRANSACTION_ERROR_INVALID_FILE,
  EAM_TRANSACTION_ERROR_NO_NETWORK,
  EAM_TRANSACTION_ERROR_PKG_UNKNOWN
} EamTransactionError;

#define EAM_TRANSACTION_ERROR eam_transaction_error_quark ()

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

  void         (* run_async) (EamTransaction *trans,
			      GCancellable *cancellable,
			      GAsyncReadyCallback callback,
			      gpointer data);

  gboolean     (* finish) (EamTransaction *trans,
			   GAsyncResult *res,
			   GError **error);
};

GType           eam_transaction_get_type (void) G_GNUC_CONST;

void            eam_transaction_run_async                          (EamTransaction *trans,
								    GCancellable *cancellable,
								    GAsyncReadyCallback callback,
								    gpointer data);

gboolean        eam_transaction_finish                             (EamTransaction *trans,
								    GAsyncResult *res,
								    GError **error);

GQuark          eam_transaction_error_quark                        (void) G_GNUC_CONST;

G_END_DECLS

#endif /* EAM_TRANSACTION_H */
