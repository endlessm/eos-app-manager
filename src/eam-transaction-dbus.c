/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include "eam-transaction-dbus.h"

#include "eam-config.h"
#include "eam-log.h"
#include "eam-install.h"
#include "eam-resources.h"
#include "eam-service.h"
#include "eam-uninstall.h"
#include "eam-update.h"

struct _EamRemoteTransaction {
  EamTransaction *transaction;
  char *obj_path;
  GCancellable *cancellable;
  EamService *service;
  GDBusConnection *connection;
  GDBusMethodInvocation *invocation;
  char *sender;
  guint registration_id;
  guint watch_id;
};

static void
eam_remote_transaction_free (EamRemoteTransaction *remote)
{
  eam_log_info_message ("Transaction '%s' is being freed.", remote->obj_path);

  if (remote->watch_id != 0)
    g_bus_unwatch_name (remote->watch_id);

  if (remote->registration_id != 0)
    g_dbus_connection_unregister_object (remote->connection,
                                         remote->registration_id);

  g_clear_object (&remote->service);
  g_clear_object (&remote->transaction);
  g_clear_object (&remote->cancellable);

  g_free (remote->sender);
  g_free (remote->obj_path);

  g_slice_free (EamRemoteTransaction, remote);
}

static void
eam_remote_transaction_cancel (EamRemoteTransaction *remote)
{
  eam_log_info_message ("Transaction '%s' was cancelled.", remote->obj_path);
  g_cancellable_cancel (remote->cancellable);

  if (remote->invocation != NULL)
    g_dbus_method_invocation_return_error_literal
      (remote->invocation, G_IO_ERROR, G_IO_ERROR_CANCELLED,
       "Transaction was cancelled");

  eam_service_pop_busy (remote->service);

  eam_remote_transaction_free (remote);
}

static void
transaction_complete_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  EamTransaction *transaction = EAM_TRANSACTION (source);
  GError *error = NULL;
  gboolean ret = eam_transaction_finish (transaction, res, &error);

  /* If we were cancelled, there's nothing left to do */
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  EamRemoteTransaction *remote = data;
  EamService *service = remote->service;
  eam_log_info_message ("Transaction '%s' result: %s", remote->obj_path, ret ? "success" : "failure");

  if (error != NULL) {
    g_dbus_method_invocation_take_error (remote->invocation, error);
  }
  else {
    GVariant *value = g_variant_new ("(b)", ret);
    g_dbus_method_invocation_return_value (remote->invocation, value);
  }

  eam_remote_transaction_free (remote);
  eam_service_pop_busy (service);
}

static void
handle_transaction_method_call (GDBusConnection *connection,
                                const char *sender,
                                const char *path,
                                const char *interface,
                                const char *method,
                                GVariant *params,
                                GDBusMethodInvocation *invocation,
                                gpointer data)
{
  EamRemoteTransaction *remote = data;

  eam_service_reset_timer (remote->service);

  if (g_strcmp0 (interface, "com.endlessm.AppManager.Transaction") != 0)
    return;

  eam_log_info_message ("Received method '%s' on transaction interface", method);

  if (g_strcmp0 (method, "CompleteTransaction") == 0) {
    const char *bundle_path = NULL, *signature_path = NULL;
    const char *storage_type = NULL;
    GVariant *properties;
    GVariantDict dict;

    g_variant_get (params, "(@a{sv})", &properties);
    g_variant_dict_init (&dict, properties);

    g_variant_dict_lookup (&dict, "StorageType", "&s", &storage_type);
    g_variant_dict_lookup (&dict, "BundlePath", "&s", &bundle_path);
    g_variant_dict_lookup (&dict, "SignaturePath", "&s", &signature_path);

    if (bundle_path != NULL && *bundle_path != '\0') {
      eam_log_info_message ("Setting bundle path to '%s' for transaction '%s'",
                            bundle_path,
                            remote->obj_path);

      const char *prefix;

      /* We use NULL to tell the transaction to use its own default */
      if (g_strcmp0 (storage_type, "primary") == 0)
        prefix = eam_config_get_primary_storage ();
      else if (g_strcmp0 (storage_type, "secondary") == 0)
        prefix = eam_config_get_secondary_storage ();
      else
        prefix = NULL;

      if (EAM_IS_INSTALL (remote->transaction)) {
        EamInstall *install = EAM_INSTALL (remote->transaction);

        eam_install_set_prefix (install, prefix);

        eam_install_set_bundle_file (install, bundle_path);
        eam_install_set_signature_file (install, signature_path);
      }
      else if (EAM_IS_UPDATE (remote->transaction)) {
        EamUpdate *update = EAM_UPDATE (remote->transaction);

        eam_update_set_prefix (update, prefix);

        eam_update_set_bundle_file (update, bundle_path);
        eam_update_set_signature_file (update, signature_path);
      }
      else {
        eam_log_error_message ("Completion of transaction is not update nor install type.");
      }
    }

    g_variant_dict_clear (&dict);

    /* we don't keep a reference here to avoid cycles */
    remote->invocation = invocation;

    eam_transaction_run_async (remote->transaction, remote->cancellable,
                               transaction_complete_cb,
                               remote);
    return;
  }

  if (g_strcmp0 (method, "CancelTransaction") == 0) {
    /* cancel the remote transaction */
    eam_remote_transaction_cancel (remote);

    g_dbus_method_invocation_return_value (invocation, NULL);

    return;
  }
}

static const GDBusInterfaceVTable transaction_vtable = {
  handle_transaction_method_call,
  NULL,
  NULL,
};

static void
eam_remote_transaction_sender_vanished (GDBusConnection *connection,
                                        const char *sender,
                                        gpointer data)
{
  EamRemoteTransaction *remote = data;

  /* if the sender disappeared while we still have a transaction
   * running, cancel the transaction
   */
  if (g_strcmp0 (remote->sender, sender) == 0) {
    eam_log_info_message ("Remote peer '%s' vanished for transaction '%s'", remote->sender, remote->obj_path);
    eam_service_reset_timer (remote->service);
    eam_remote_transaction_cancel (remote);
  }
}

static GDBusNodeInfo*
load_introspection (const char *name, GError **error)
{
  g_resources_register (eam_get_resource ());

  char *path = g_strconcat ("/com/endlessm/AppManager/", name, NULL);
  GBytes *data = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, error);
  g_free (path);

  if (data == NULL)
    return NULL;

  GDBusNodeInfo *info = g_dbus_node_info_new_for_xml (g_bytes_get_data (data, NULL), error);
  g_bytes_unref (data);

  return info;
}

static gboolean
eam_remote_transaction_register_dbus (EamRemoteTransaction *remote,
                                      GDBusConnection *connection,
                                      GError **error)
{
  static GDBusNodeInfo *transaction = NULL;
  GError *internal_error = NULL;

  if (transaction == NULL) {
    transaction = load_introspection ("eam-transaction-interface.xml", &internal_error);

    if (internal_error != NULL) {
      eam_log_error_message ("Unable to load 'eam-transaction-interface.xml': %s",
                             internal_error->message);
      g_propagate_error (error, internal_error);
      return FALSE;
    }
  }

  remote->connection = connection;
  remote->registration_id =
    g_dbus_connection_register_object (remote->connection,
                                       remote->obj_path,
                                       transaction->interfaces[0],
                                       &transaction_vtable,
                                       remote, NULL,
                                       &internal_error);

  if (internal_error != NULL) {
    eam_log_error_message ("Unable to register object '%s': %s",
                           remote->obj_path,
                           internal_error->message);
    g_propagate_error (error, internal_error);
    return FALSE;
  }

  remote->watch_id =
    g_bus_watch_name_on_connection (remote->connection,
                                    remote->sender,
                                    G_BUS_NAME_WATCHER_FLAGS_NONE,
                                    NULL,
                                    eam_remote_transaction_sender_vanished,
                                    remote,
                                    NULL);

  return TRUE;
}

EamRemoteTransaction *
eam_remote_transaction_new (EamService *service,
                            GDBusConnection *connection,
                            const char *sender,
                            EamTransaction *transaction,
                            GError **error)
{
  EamRemoteTransaction *res = g_slice_new0 (EamRemoteTransaction);

  res->service = g_object_ref (service);
  res->transaction = g_object_ref (transaction);
  res->obj_path = eam_service_get_next_transaction_path (service);
  res->sender = g_strdup (sender);
  res->cancellable = g_cancellable_new ();

  if (!eam_remote_transaction_register_dbus (res, connection, error)) {
    eam_remote_transaction_free (res);
    return NULL;
  }

  return res;
}

const char *
eam_remote_transaction_get_obj_path (EamRemoteTransaction *remote)
{
  return remote->obj_path;
}
