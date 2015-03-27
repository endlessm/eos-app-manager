/* Copyright 2014 Endless Mobile, Inc. */
#include "config.h"

#include <glib/gi18n.h>
#include <polkit/polkit.h>

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "eam-error.h"
#include "eam-service.h"
#include "eam-install.h"
#include "eam-update.h"
#include "eam-uninstall.h"
#include "eam-dbus-utils.h"
#include "eam-log.h"
#include "eam-resources.h"

typedef struct _EamServicePrivate EamServicePrivate;

struct _EamServicePrivate {
  GDBusConnection *connection;
  guint registration_id;

  EamTransaction *trans;
  GQueue *invocation_queue;

  GHashTable *active_transactions;

  GTimer *timer;

  GCancellable *cancellable;

  PolkitAuthority *authority;

  guint64 last_transaction;
  gboolean authorizing;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamService, eam_service, G_TYPE_OBJECT)

typedef enum {
  EAM_SERVICE_METHOD_INSTALL,
  EAM_SERVICE_METHOD_UPDATE,
  EAM_SERVICE_METHOD_UNINSTALL,
  EAM_SERVICE_METHOD_USER_CAPS
} EamServiceMethod;

typedef void (*EamServiceRun) (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params);

typedef struct  {
  EamServiceMethod method;
  const gchar *dbus_name;
  EamServiceRun run;
  const gchar *action_id;
  const gchar *message;
} EamServiceAuth;

typedef struct {
  EamService *service;
  GDBusMethodInvocation *invocation;
  EamServiceMethod method;
  GVariant *params;
} EamInvocationInfo;

typedef struct {
  EamTransaction *transaction;
  char *obj_path;
  GCancellable *cancellable;
  EamService *service;
  GDBusConnection *connection;
  GDBusMethodInvocation *invocation;
  char *sender;
  guint registration_id;
  guint watch_id;
} EamRemoteTransaction;

static void eam_service_install (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params);
static void eam_service_update (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params);
static void eam_service_uninstall (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params);
static void eam_service_get_user_caps (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params);

static void run_method_with_authorization (EamService *service, GDBusMethodInvocation *invocation,
  EamServiceMethod method, GVariant *params);

static void eam_service_remove_active_transaction (EamService *service,
                                                   EamRemoteTransaction *remote);

static EamRemoteTransaction *eam_remote_transaction_new (EamService *service,
                                                         const char *sender,
                                                         EamTransaction *transaction,
                                                         GError **error);
static void eam_remote_transaction_cancel (EamRemoteTransaction *remote);
static void eam_remote_transaction_free (EamRemoteTransaction *remote);

static EamServiceAuth auth_action[] = {
  [EAM_SERVICE_METHOD_INSTALL] = {
    .method = EAM_SERVICE_METHOD_INSTALL,
    .dbus_name = "Install",
    .run = eam_service_install,
    .action_id = "com.endlessm.app-installer.install-application",
    .message = N_("Authentication is required to install software"),
  },

  [EAM_SERVICE_METHOD_UPDATE] = {
    .method = EAM_SERVICE_METHOD_UPDATE,
    .dbus_name = "Update",
    .run = eam_service_update,
    .action_id = "com.endlessm.app-installer.update-application",
    .message = N_("Authentication is required to update software"),
  },

  [EAM_SERVICE_METHOD_UNINSTALL] = {
    .method = EAM_SERVICE_METHOD_UNINSTALL,
    .dbus_name = "Uninstall",
    .run = eam_service_uninstall,
    .action_id = "com.endlessm.app-installer.uninstall-application",
    .message = N_("Authentication is required to uninstall software"),
  },

  [EAM_SERVICE_METHOD_USER_CAPS] = {
    .method = EAM_SERVICE_METHOD_USER_CAPS,
    .dbus_name = "GetUserCapabilities",
    .run = eam_service_get_user_caps,
    .action_id = NULL,
    .message = "",
  },
};

static inline void
eam_service_perf_mark (const char *format,
                       ...)
{
  static gint64 last_debug_stamp;
  va_list args;

  gint64 cur_time = g_get_monotonic_time ();
  gint64 debug_stamp;

  char *stamp;

  if (last_debug_stamp == 0 ||
      cur_time - last_debug_stamp >= (5 * G_USEC_PER_SEC)) {
    debug_stamp = cur_time;
    last_debug_stamp = debug_stamp;
    stamp = g_strdup_printf ("[%16" G_GINT64_FORMAT "]", debug_stamp);
  }
  else {
    debug_stamp = cur_time - last_debug_stamp;
    stamp = g_strdup_printf ("[%+16" G_GINT64_FORMAT "]", debug_stamp);
  }

  char *fmt = g_strconcat (stamp, ":", format, NULL);
  g_free (stamp);

  char *str;

  va_start (args, format);
  str = g_strdup_vprintf (fmt, args);
  g_free (fmt);
  va_end (args);

  eam_log_info_message (str, NULL);
  g_free (str);
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

static EamInvocationInfo *
eam_invocation_info_new (EamService *service,
                         GDBusMethodInvocation *invocation,
                         EamServiceMethod method,
                         GVariant *params)
{
  EamInvocationInfo *info;

  info = g_slice_new0 (EamInvocationInfo);
  info->service = service;
  info->invocation = invocation;
  info->method = method;
  info->params = params;

  return info;
}

static void
eam_invocation_info_free (EamInvocationInfo *info)
{
  g_slice_free (EamInvocationInfo, info);
}

static void
eam_service_dispose (GObject *obj)
{
  EamServicePrivate *priv = eam_service_get_instance_private (EAM_SERVICE (obj));

  if (priv->connection) {
    g_dbus_connection_unregister_object (priv->connection, priv->registration_id);
    priv->registration_id = 0;
    priv->connection = NULL;
  }

  g_clear_pointer (&priv->active_transactions, g_hash_table_unref);
  g_clear_pointer (&priv->timer, g_timer_destroy);

  if (priv->invocation_queue) {
    g_queue_free_full (priv->invocation_queue, (GDestroyNotify) eam_invocation_info_free);
    priv->invocation_queue = NULL;
  }

  g_clear_object (&priv->cancellable);
  g_clear_object (&priv->authority);

  G_OBJECT_CLASS (eam_service_parent_class)->dispose (obj);
}

static void
eam_service_class_init (EamServiceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = eam_service_dispose;
}

static void
eam_service_init (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  priv->cancellable = g_cancellable_new ();
  priv->timer = g_timer_new ();
  priv->invocation_queue = g_queue_new ();
}

/**
 * eam_service_new:
 *
 * Returns: Creates a new #EamService instance.
 **/
EamService *
eam_service_new (void)
{
  return g_object_new (EAM_TYPE_SERVICE, NULL);
}

static void
eam_service_add_active_transaction (EamService *service,
                                    EamRemoteTransaction *remote)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (priv->active_transactions == NULL)
    priv->active_transactions = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                       NULL, // the key is in the value
                                                       (GDestroyNotify) eam_remote_transaction_free);

  g_hash_table_replace (priv->active_transactions, remote->obj_path, remote);
}

static void
eam_service_remove_active_transaction (EamService *service,
                                       EamRemoteTransaction *remote)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  eam_log_info_message ("Remove active transaction '%s'", remote->obj_path);

  if (priv->active_transactions == NULL ||
      !g_hash_table_remove (priv->active_transactions, remote->obj_path))
    eam_log_error_message ("Asked to remove transaction '%s'[%p] without adding it first.",
                remote->obj_path,
                remote);
}

static void
eam_service_reset_timer (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  g_timer_reset (priv->timer);
}

static void
eam_service_check_queue (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);
  EamInvocationInfo *info = g_queue_pop_head (priv->invocation_queue);

  if (info == NULL)
    return;

  run_method_with_authorization (service, info->invocation, info->method, info->params);
  eam_invocation_info_free (info);
}

static void
eam_service_clear_transaction (EamService *service,
                               EamTransaction *transaction)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  /* if the transaction is the one currently running, then we remove it */
  if (priv->trans == transaction) {
    g_clear_object (&priv->trans);

    /* do not need the cancellable any more */
    g_clear_object (&priv->cancellable);
    priv->cancellable = g_cancellable_new ();
  }

  eam_service_check_queue (service);
}

typedef void (*EamRunService) (EamService *service, const gpointer params,
  GDBusMethodInvocation *invocation);

static EamRemoteTransaction *
start_dbus_transaction (EamService *service,
                        GDBusMethodInvocation *invocation,
                        GError **error)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);
  const char *sender = g_dbus_method_invocation_get_sender (invocation);

  GError *internal_error = NULL;
  EamRemoteTransaction *remote = eam_remote_transaction_new (service, sender,
                                                             priv->trans,
                                                             &internal_error);

  if (internal_error != NULL) {
    g_set_error (error, EAM_ERROR,
                 EAM_ERROR_UNIMPLEMENTED,
                 _("Internal transaction error: %s"),
                 internal_error->message);
    g_clear_object (&priv->trans);
    g_clear_error (&internal_error);

    return NULL;
  }

  eam_service_add_active_transaction (service, remote);

  return remote;
}

static void
eam_service_install (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params)
{
  eam_service_reset_timer (service);

  const char *appid = NULL;
  g_variant_get (params, "(&s)", &appid);

  eam_log_info_message ("Install() method called (appid: %s)", appid);

  EamServicePrivate *priv = eam_service_get_instance_private (service);

  priv->trans = eam_install_new (appid);

  GError *error = NULL;
  EamRemoteTransaction *remote_transaction = NULL;
  remote_transaction = start_dbus_transaction (service, invocation, &error);

  if (remote_transaction != NULL) {
    GVariant *res = g_variant_new ("(o)", remote_transaction->obj_path);
    g_dbus_method_invocation_return_value (invocation, res);
  }
  else {
    g_dbus_method_invocation_return_gerror (invocation, error);
    g_error_free (error);

    eam_service_check_queue (service);
  }
}

static void
eam_service_update (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params)
{
  eam_service_reset_timer (service);

  const char *appid = NULL;
  gboolean allow_deltas = FALSE;

  g_variant_get (params, "(&sb)", &appid, &allow_deltas);

  eam_log_info_message ("Update service called (appid: %s, deltas: %s)", appid,
                        allow_deltas ? "True" : "False");

  EamServicePrivate *priv = eam_service_get_instance_private (service);

  priv->trans = eam_update_new (appid, allow_deltas);

  GError *error = NULL;
  EamRemoteTransaction *remote_transaction = NULL;
  remote_transaction = start_dbus_transaction (service, invocation, &error);

  if (remote_transaction != NULL) {
    GVariant *res = g_variant_new ("(o)", remote_transaction->obj_path);
    g_dbus_method_invocation_return_value (invocation, res);
  }
  else {
    g_dbus_method_invocation_return_gerror (invocation, error);
    g_error_free (error);

    eam_service_check_queue (service);
  }
}

static void
uninstall_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  GDBusMethodInvocation *invocation = g_object_get_data (source, "invocation");
  g_assert (invocation);

  EamService *service = EAM_SERVICE (data);
  EamServicePrivate *priv = eam_service_get_instance_private (service);
  g_assert (source == G_OBJECT (priv->trans));

  GError *error = NULL;
  gboolean ret = eam_transaction_finish (priv->trans, res, &error);
  if (error) {
    g_dbus_method_invocation_take_error (invocation, error);
    goto out;
  }

  g_object_set_data (source, "invocation", NULL);
  GVariant *value = g_variant_new ("(b)", ret);
  g_dbus_method_invocation_return_value (invocation, value);

out:
  eam_service_clear_transaction (service, priv->trans);
}

static void
eam_service_uninstall (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params)
{
  char *appid = NULL;
  g_variant_get (params, "(&s)", &appid);

  EamServicePrivate *priv = eam_service_get_instance_private (service);

  priv->trans = eam_uninstall_new (appid);
  g_object_set_data (G_OBJECT (priv->trans), "invocation", invocation);

  eam_transaction_run_async (priv->trans, priv->cancellable, uninstall_cb, service);
}

static gboolean
user_is_in_admin_group (uid_t user, const char *admin_group)
{
  if (user == G_MAXUINT)
    return FALSE;

  struct passwd *pw = getpwuid (user);
  if (pw == NULL)
    return FALSE;

  int n_groups = 10;
  gid_t *groups = g_new0 (gid_t, n_groups);

  while (1)
    {
      int max_n_groups = n_groups;
      int ret = getgrouplist (pw->pw_name, pw->pw_gid, groups, &max_n_groups);

      if (ret >= 0)
        {
          n_groups = max_n_groups;
          break;
        }

      /* some systems fail to update n_groups so we just grow it by approximation */
      if (n_groups == max_n_groups)
        n_groups = 2 * max_n_groups;
      else
        n_groups = max_n_groups;

      groups = g_renew (gid_t, groups, n_groups);
    }

  gboolean retval = FALSE;
  int i = 0;

  for (i = 0; i < n_groups; i++)
    {
      struct group *gr = getgrgid (groups[i]);

      if (gr != NULL && strcmp (gr->gr_name, admin_group) == 0)
        {
          retval = TRUE;
          break;
        }
    }

  g_free (groups);

  if (!retval)
    eam_log_error_message("Matching admin group not found in invocation");

  return retval;
}

static gboolean
eam_service_check_auth_by_method (EamService *service, PolkitSubject *subject,
  EamServiceMethod method)
{
  GError *error = NULL;
  gboolean ret = FALSE;
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  const gchar *action = auth_action[method].action_id;
  PolkitAuthorizationResult *result =
    polkit_authority_check_authorization_sync (priv->authority, subject,
      action, NULL, 0, NULL, &error);

  if (error) {
    eam_log_error_message ("Unable to check authorisation for %s: %s", action,
      error->message);
    g_clear_error (&error);
    goto bail;
  }

  ret = polkit_authorization_result_get_is_authorized (result) ||
    polkit_authorization_result_get_is_challenge (result);

bail:
  g_object_unref (result);

  return ret;
}

static void
eam_service_get_user_caps (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  GVariantBuilder builder;

  const char *sender = g_dbus_method_invocation_get_sender (invocation);
  uid_t user = eam_dbus_get_uid_for_sender (priv->connection, sender);

  gboolean can_install = FALSE;
  gboolean can_uninstall = FALSE;

  /* XXX: we want to have a separate configuration to decide the capabilities
   * for each user, but for the time being we can use the 'wheel' group to
   * decide if a user has the capabilities to install/update/remove apps.
   */
  if (user_is_in_admin_group (user, EAM_ADMIN_GROUP_NAME)) {
    can_install = TRUE;
    can_uninstall = TRUE;
    goto out;
  }

  /* if the user is not in the ADMIN_GROUP_NAME then we go through
   * a polkit check
   */
  PolkitSubject *subject = polkit_system_bus_name_new (sender);
  if (!subject) {
    eam_log_error_message ("Unable to create the Polkit subject for: %s", sender);
    goto out;
  }

  can_install = eam_service_check_auth_by_method (service, subject,
    EAM_SERVICE_METHOD_INSTALL);
  can_uninstall = eam_service_check_auth_by_method (service, subject,
    EAM_SERVICE_METHOD_UNINSTALL);

out:
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(a{sv})"));
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));

  g_variant_builder_add (&builder, "{sv}", "CanInstall", g_variant_new_boolean (can_install));
  g_variant_builder_add (&builder, "{sv}", "CanUninstall", g_variant_new_boolean (can_uninstall));

  g_variant_builder_close (&builder);
  GVariant *res = g_variant_builder_end (&builder);
  g_dbus_method_invocation_return_value (invocation, res);

  eam_service_check_queue (service);
}

static void
check_authorization_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  PolkitAuthorizationResult *result;
  GError *error = NULL;
  EamInvocationInfo *info = data;
  EamServicePrivate *priv = eam_service_get_instance_private (info->service);
  g_assert (source == G_OBJECT (priv->authority));

  priv->authorizing = FALSE;

  result = polkit_authority_check_authorization_finish (priv->authority, res, &error);

  if (result == NULL) {
    g_dbus_method_invocation_take_error (info->invocation, error);
    goto bail;
  }

  /* Check if the operation was cancelled */
  if (g_cancellable_set_error_if_cancelled (priv->cancellable, &error)) {
    g_dbus_method_invocation_take_error (info->invocation, error);
    goto bail;
  }

  /* Did not auth */
  if (!polkit_authorization_result_get_is_authorized (result)) {
    g_dbus_method_invocation_return_error (info->invocation, EAM_ERROR,
      EAM_ERROR_NOT_AUTHORIZED, _("Not authorized to perform the operation"));
    goto bail;
  }

  if (auth_action[info->method].run)
    auth_action[info->method].run (info->service, info->invocation, info->params);

bail:
  eam_invocation_info_free (info);
  if (result)
    g_object_unref (result);
}

static void
run_method_with_authorization (EamService *service, GDBusMethodInvocation *invocation,
                               EamServiceMethod method, GVariant *params)
{
  if (!auth_action[method].action_id) {
    /* The service does not any require authorization */
    auth_action[method].run (service, invocation, params);
    return;
  }

  const gchar *sender = g_dbus_method_invocation_get_sender (invocation);
  PolkitSubject *subject = polkit_system_bus_name_new (sender);
  if (subject == NULL) {
    eam_log_error_message ("Unable to create the Polkit subject for: %s", sender);
    g_dbus_method_invocation_return_error (invocation, EAM_ERROR,
                                           EAM_ERROR_AUTHORIZATION,
                                           _("An error happened during the authorization process"));
    return;
  }

  EamServicePrivate *priv = eam_service_get_instance_private (service);
  priv->authorizing = TRUE;

  PolkitDetails *details = polkit_details_new ();
  polkit_details_insert (details, "polkit.message", auth_action[method].message);
  polkit_details_insert (details, "polkit.gettext_domain", GETTEXT_PACKAGE);

  EamInvocationInfo *info = eam_invocation_info_new (service, invocation, method, params);
  polkit_authority_check_authorization (priv->authority, subject, auth_action[method].action_id,
    details, POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION, priv->cancellable,
    (GAsyncReadyCallback) check_authorization_cb, info);

  g_object_unref (subject);
  g_object_unref (details);
}

static gboolean
eam_service_is_busy (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  return (priv->trans || priv->authorizing);
}

static void
eam_service_run (EamService *service, GDBusMethodInvocation *invocation,
                 EamServiceMethod method, GVariant *params)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  eam_service_reset_timer (service);

  if (eam_service_is_busy (service)) {
    EamInvocationInfo *info = eam_invocation_info_new (service, invocation, method, params);
    g_queue_push_tail (priv->invocation_queue, info);

    return;
  }

  run_method_with_authorization (service, invocation, method, params);
}

static void
eam_remote_transaction_free (EamRemoteTransaction *remote)
{
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
  eam_service_remove_active_transaction (remote->service, remote);
}

static void
transaction_complete_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  EamRemoteTransaction *remote = data;
  EamService *service = remote->service;
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  GError *error = NULL;
  gboolean ret = eam_transaction_finish (remote->transaction, res, &error);
  if (error) {
    g_dbus_method_invocation_take_error (remote->invocation, error);
    goto out;
  }

  eam_log_info_message ("Transaction '%s' result: %s", remote->obj_path, ret ? "success" : "failure");

  GVariant *value = g_variant_new ("(b)", ret);
  g_dbus_method_invocation_return_value (remote->invocation, value);

out:
  eam_service_remove_active_transaction (service, remote);
  eam_service_clear_transaction (service, priv->trans);
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
    char *bundle_path;

    g_variant_get (params, "(s)", &bundle_path);

    if (bundle_path != NULL && *bundle_path != '\0') {
      eam_log_info_message ("Setting bundle path to '%s' for transaction '%s'",
                            bundle_path,
                            remote->obj_path);

      if (EAM_IS_INSTALL (remote->transaction)) {
        EamInstall *install = EAM_INSTALL (remote->transaction);
        eam_install_set_bundle_location (install, bundle_path);
      } else if (EAM_IS_UPDATE (remote->transaction)) {
        EamUpdate *update = EAM_UPDATE (remote->transaction);
        eam_update_set_bundle_location (update, bundle_path);
      } else {
        eam_log_error_message ("Completion of transaction is not update nor install type.");
      }

      g_free (bundle_path);
    }

    /* we don't keep a reference here to avoid cycles */
    remote->invocation = invocation;

    eam_transaction_run_async (remote->transaction, remote->cancellable,
                               transaction_complete_cb,
                               remote);
    return;
  }

  if (g_strcmp0 (method, "CancelTransaction") == 0) {
    /* clear the transaction from the queue */
    eam_service_clear_transaction (remote->service, remote->transaction);

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

static char *
get_next_transaction_path (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  return g_strdup_printf ("/com/endlessm/AppStore/Transactions/%" G_GUINT64_FORMAT,
                          priv->last_transaction++);
}

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
    eam_service_clear_transaction (remote->service, remote->transaction);
    eam_remote_transaction_cancel (remote);
  }
}

static gboolean
eam_remote_transaction_register_dbus (EamRemoteTransaction *remote,
                                      EamService *service,
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

  EamServicePrivate *priv = eam_service_get_instance_private (service);

  remote->registration_id =
    g_dbus_connection_register_object (priv->connection,
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

  remote->connection = priv->connection;
  remote->watch_id =
    g_bus_watch_name_on_connection (priv->connection,
                                    remote->sender,
                                    G_BUS_NAME_WATCHER_FLAGS_NONE,
                                    NULL,
                                    eam_remote_transaction_sender_vanished,
                                    remote,
                                    NULL);

  return TRUE;
}

static EamRemoteTransaction *
eam_remote_transaction_new (EamService *service,
                            const char *sender,
                            EamTransaction *transaction,
                            GError **error)
{
  EamRemoteTransaction *res = g_slice_new0 (EamRemoteTransaction);

  res->service = g_object_ref (service);
  res->transaction = g_object_ref (transaction);
  res->obj_path = get_next_transaction_path (service);
  res->sender = g_strdup (sender);
  res->cancellable = g_cancellable_new ();

  if (!eam_remote_transaction_register_dbus (res, service, error)) {
    eam_remote_transaction_free (res);
    return NULL;
  }

  return res;
}

static void
handle_method_call (GDBusConnection *connection, const char *sender,
  const char *path, const char *interface, const char *method, GVariant *params,
  GDBusMethodInvocation *invocation, gpointer data)
{
  EamService *service = EAM_SERVICE (data);
  int method_i;

  if (g_strcmp0 (interface, "com.endlessm.AppManager"))
    return;

  eam_log_info_message ("Received method '%s' on manager interface", method);

  for (method_i = 0; method_i < G_N_ELEMENTS (auth_action); method_i++) {
    if (g_strcmp0 (method, auth_action[method_i].dbus_name) == 0) {
      eam_service_run (service, invocation, method_i, params);
      break;
    }
  }
}

static const GDBusInterfaceVTable service_vtable = {
  handle_method_call,
  NULL,
  NULL,
};

/**
 * eam_service_dbus_register:
 * @service: a #EamService instance.
 * @connection: (transfer full): a #GDBusConnection instance.
 *
 * Registers the #EamService service in the DBus connection.
 **/
void
eam_service_dbus_register (EamService *service, GDBusConnection *connection)
{
  static GDBusNodeInfo *service_info = NULL;

  g_return_if_fail (EAM_IS_SERVICE (service));
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));

  GError *error = NULL;

  if (service_info == NULL) {
    service_info = load_introspection ("eam-service-interface.xml", &error);

    if (error) {
      eam_log_error_message ("Failed to load DBus introspection: %s", error->message);
      g_error_free (error);
      return;
    }
  }

  EamServicePrivate *priv = eam_service_get_instance_private (service);
  priv->registration_id = g_dbus_connection_register_object (connection,
    "/com/endlessm/AppManager", service_info->interfaces[0], &service_vtable,
    service, NULL, &error);

  if (!priv->registration_id) {
    eam_log_error_message ("Failed to register service object: %s\n", error->message);
    g_error_free (error);
    return;
  }

  priv->connection = connection;
  g_object_add_weak_pointer (G_OBJECT (connection), (gpointer *) &priv->connection);
}

/**
 * eam_service_load_authority:
 * @service: a #EamService instance
 *
 * Returns: %TRUE if the #PolkitAuthority was successfully obtained,
 * %FALSE otherwise
 **/
gboolean
eam_service_load_authority (EamService *service, GError **error)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);
  priv->authority = polkit_authority_get_sync (NULL, error);

  return (priv->authority != NULL);
}

/**
 * eam_service_get_idle:
 * @service: a #EamService instance
 *
 * Returns: the idle elapsed time in seconds
 **/
guint
eam_service_get_idle (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  eam_log_info_message ("Service is %s - elapsed since last reset: %.2f seconds",
                        eam_service_is_busy (service) ? "busy" : "not busy",
                        g_timer_elapsed (priv->timer, NULL));

  if (eam_service_is_busy (service))
    return 0;

  return g_timer_elapsed (priv->timer, NULL);
}
