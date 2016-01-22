/* eam-server.c: DBus server implementation
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

#include <glib/gi18n.h>
#include <polkit/polkit.h>

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "eam-config.h"
#include "eam-dbus-utils.h"
#include "eam-error.h"
#include "eam-install.h"
#include "eam-log.h"
#include "eam-resources.h"
#include "eam-service.h"
#include "eam-transaction-dbus.h"
#include "eam-uninstall.h"
#include "eam-update.h"
#include "eam-utils.h"

typedef struct _EamServicePrivate EamServicePrivate;

struct _EamServicePrivate {
  guint busy_counter;

  GTimer *timer;

  PolkitAuthority *authority;

  guint64 last_transaction;
};

static void eam_app_manager_iface_init (EamAppManagerIface *iface);

G_DEFINE_TYPE_WITH_CODE (EamService, eam_service, EAM_TYPE_APP_MANAGER_SKELETON,
                         G_ADD_PRIVATE (EamService)
                         G_IMPLEMENT_INTERFACE (EAM_TYPE_APP_MANAGER, eam_app_manager_iface_init));

typedef struct {
  const char *action_id;
  const char *message;
} EamAuthAction;

static EamAuthAction auth_action_install = {
  .action_id = "com.endlessm.app-installer.install-application",
  .message = N_("Authentication is required to install software"),
};
static EamAuthAction auth_action_uninstall = {
  .action_id = "com.endlessm.app-installer.uninstall-application",
  .message = N_("Authentication is required to uninstall software"),
};
static EamAuthAction auth_action_update = {
  .action_id = "com.endlessm.app-installer.update-application",
  .message = N_("Authentication is required to update software"),
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

static void
eam_service_dispose (GObject *obj)
{
  EamServicePrivate *priv = eam_service_get_instance_private (EAM_SERVICE (obj));

  g_clear_pointer (&priv->timer, g_timer_destroy);

  g_clear_object (&priv->authority);

  G_OBJECT_CLASS (eam_service_parent_class)->dispose (obj);
}

static void
eam_service_constructed (GObject *obj)
{
  G_OBJECT_CLASS (eam_service_parent_class)->constructed (obj);

  g_object_set (obj,
                "applications-dir", eam_config_get_applications_dir (),
                "primary-storage", eam_config_get_primary_storage (),
                "secondary-storage", eam_config_get_secondary_storage (),
                "server-url", eam_config_get_server_url (),
                "api-version", eam_config_get_api_version (),
                "enable-delta-updates", eam_config_get_enable_delta_updates (),
                NULL);
}

static void
eam_service_class_init (EamServiceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = eam_service_constructed;
  object_class->dispose = eam_service_dispose;
}

static void
eam_service_init (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  priv->timer = g_timer_new ();
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
eam_service_push_busy (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);
  priv->busy_counter++;
}

void
eam_service_pop_busy (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);
  priv->busy_counter--;
}

void
eam_service_reset_timer (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  g_timer_reset (priv->timer);
}

char *
eam_service_get_next_transaction_path (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  return g_strdup_printf ("/com/endlessm/AppStore/Transactions/%" G_GUINT64_FORMAT,
                          priv->last_transaction++);
}

typedef void (*EamRunService) (EamService *service, const gpointer params,
  GDBusMethodInvocation *invocation);

static EamRemoteTransaction *
start_dbus_transaction (EamService *service,
			EamTransaction *trans,
                        GDBusMethodInvocation *invocation,
                        GError **error)
{
  const char *sender = g_dbus_method_invocation_get_sender (invocation);

  GError *internal_error = NULL;
  EamRemoteTransaction *remote = eam_remote_transaction_new (service,
                                                             g_dbus_interface_skeleton_get_connection (G_DBUS_INTERFACE_SKELETON (service)),
                                                             sender,
							     trans,
                                                             &internal_error);

  if (internal_error != NULL) {
    g_set_error (error, EAM_ERROR,
                 EAM_ERROR_UNIMPLEMENTED,
                 _("Internal transaction error: %s"),
                 internal_error->message);
    g_clear_error (&internal_error);

    return NULL;
  }

  eam_service_push_busy (service);

  return remote;
}

static gboolean
eam_service_check_auth_full (EamService *service, GDBusMethodInvocation *invocation,
                             EamAuthAction *auth_action)
{
  const gchar *sender = g_dbus_method_invocation_get_sender (invocation);
  PolkitSubject *subject = polkit_system_bus_name_new (sender);
  PolkitDetails *details = NULL;
  PolkitAuthorizationResult *result = NULL;
  gboolean ret = FALSE;

  if (subject == NULL) {
    eam_log_error_message ("Unable to create the Polkit subject for: %s", sender);
    g_dbus_method_invocation_return_error (invocation, EAM_ERROR,
                                           EAM_ERROR_AUTHORIZATION,
                                           _("An error happened during the authorization process"));
    goto out;
  }

  EamServicePrivate *priv = eam_service_get_instance_private (service);

  details = polkit_details_new ();
  polkit_details_insert (details, "polkit.message", auth_action->message);
  polkit_details_insert (details, "polkit.gettext_domain", GETTEXT_PACKAGE);

  GError *error = NULL;
  result =
    polkit_authority_check_authorization_sync (priv->authority, subject, auth_action->action_id,
                                               details, POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
                                               NULL, &error);

  if (error) {
    eam_log_error_message ("Unable to check authorisation for %s: %s", auth_action->action_id,
      error->message);
    g_dbus_method_invocation_return_error (invocation, EAM_ERROR,
      EAM_ERROR_NOT_AUTHORIZED, _("Not authorized to perform the operation"));
    g_clear_error (&error);
    goto out;
  }

  /* Did not auth */
  if (!polkit_authorization_result_get_is_authorized (result)) {
    g_dbus_method_invocation_return_error (invocation, EAM_ERROR,
      EAM_ERROR_NOT_AUTHORIZED, _("Not authorized to perform the operation"));
    goto out;
  }

  ret = TRUE;

 out:
  g_clear_object (&subject);
  g_clear_object (&details);
  g_clear_object (&result);
  return ret;
}

static gboolean
eam_service_check_auth (EamService *service, PolkitSubject *subject, EamAuthAction *auth_action)
{
  GError *error = NULL;
  gboolean ret = FALSE;
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  PolkitAuthorizationResult *result =
    polkit_authority_check_authorization_sync (priv->authority, subject,
      auth_action->action_id, NULL, 0, NULL, &error);

  if (error) {
    eam_log_error_message ("Unable to check authorisation for %s: %s", auth_action->action_id,
      error->message);
    g_clear_error (&error);
    goto bail;
  }

  ret = polkit_authorization_result_get_is_authorized (result) ||
    polkit_authorization_result_get_is_challenge (result);

bail:
  g_clear_object (&result);

  return ret;
}

static gboolean
handle_install (EamAppManager *object, GDBusMethodInvocation *invocation, const char *appid)
{
  EamService *service = EAM_SERVICE (object);

  if (!eam_service_check_auth_full (service, invocation, &auth_action_install))
    return TRUE;

  eam_service_reset_timer (service);

  eam_log_info_message ("Install() method called (appid: %s)", appid);

  EamTransaction *trans = eam_install_new (appid);
  GError *error = NULL;
  EamRemoteTransaction *remote_transaction =
    start_dbus_transaction (service, trans, invocation, &error);
  g_object_unref (trans);

  if (remote_transaction != NULL) {
    eam_app_manager_complete_install (object, invocation,
                                      eam_remote_transaction_get_obj_path (remote_transaction));
  }
  else {
    g_dbus_method_invocation_return_gerror (invocation, error);
    g_error_free (error);
  }

  return TRUE;
}

static gboolean
handle_update (EamAppManager *object, GDBusMethodInvocation *invocation, const char *appid)
{
  EamService *service = EAM_SERVICE (object);

  if (!eam_service_check_auth_full (service, invocation, &auth_action_update))
    return TRUE;

  eam_service_reset_timer (service);

  eam_log_info_message ("Update service called (appid: %s)", appid);

  EamTransaction *trans = eam_update_new (appid);
  GError *error = NULL;
  EamRemoteTransaction *remote_transaction =
    start_dbus_transaction (service, trans, invocation, &error);
  g_object_unref (trans);

  if (remote_transaction != NULL) {
    eam_app_manager_complete_update (object, invocation,
                                     eam_remote_transaction_get_obj_path (remote_transaction));
  }
  else {
    g_dbus_method_invocation_return_gerror (invocation, error);
    g_error_free (error);
  }

  return TRUE;
}

static void
uninstall_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  EamService *service = data;
  EamTransaction *trans = EAM_TRANSACTION (source);
  GDBusMethodInvocation *invocation = g_object_get_data (source, "invocation");
  g_assert (invocation);

  g_object_set_data (source, "invocation", NULL);

  GError *error = NULL;
  gboolean ret = eam_transaction_finish (trans, res, &error);
  if (error) {
    g_dbus_method_invocation_take_error (invocation, error);
  }
  else {
    eam_app_manager_complete_uninstall (EAM_APP_MANAGER (service), invocation, ret);
  }

  eam_service_pop_busy (service);
}

static gboolean
handle_uninstall (EamAppManager *object, GDBusMethodInvocation *invocation, const char *appid)
{
  EamService *service = EAM_SERVICE (object);

  if (!eam_service_check_auth_full (service, invocation, &auth_action_uninstall))
    return TRUE;

  EamTransaction *trans = eam_uninstall_new (appid);
  g_object_set_data (G_OBJECT (trans), "invocation", invocation);

  eam_transaction_run_async (trans, NULL, uninstall_cb, service);
  g_object_unref (trans);

  eam_service_push_busy (service);

  return TRUE;
}

static gboolean
handle_get_user_capabilities (EamAppManager *object,
                              GDBusMethodInvocation *invocation)
{
  EamService *service = EAM_SERVICE (object);

  GVariantBuilder builder;

  const char *sender = g_dbus_method_invocation_get_sender (invocation);
  uid_t user = eam_dbus_get_uid_for_sender (g_dbus_interface_skeleton_get_connection (G_DBUS_INTERFACE_SKELETON (service)), sender);

  gboolean can_install = FALSE;
  gboolean can_uninstall = FALSE;

  /* XXX: we want to have a separate configuration to decide the capabilities
   * for each user, but for the time being we can use the EAM_ADMIN_GROUP_NAME
   * group to decide if a user has the capabilities to install/update/remove
   * apps.
   */
  if (eam_utils_check_unix_permissions (user)) {
    can_install = TRUE;
    can_uninstall = TRUE;
    goto out;
  }

  /* if the user is not in the EAM_ADMIN_GROUP_NAME then we go through
   * a polkit check
   */
  PolkitSubject *subject = polkit_system_bus_name_new (sender);
  if (!subject) {
    eam_log_error_message ("Unable to create the Polkit subject for: %s", sender);
    goto out;
  }

  can_install = eam_service_check_auth (service, subject, &auth_action_install);
  can_uninstall = eam_service_check_auth (service, subject, &auth_action_uninstall);

out:
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  g_variant_builder_add (&builder, "{sv}", "CanInstall", g_variant_new_boolean (can_install));
  g_variant_builder_add (&builder, "{sv}", "CanUninstall", g_variant_new_boolean (can_uninstall));

  eam_app_manager_complete_get_user_capabilities (object, invocation,
                                                  g_variant_builder_end (&builder));
  return TRUE;
}

static void
eam_app_manager_iface_init (EamAppManagerIface *iface)
{
  iface->handle_get_user_capabilities = handle_get_user_capabilities;
  iface->handle_install = handle_install;
  iface->handle_uninstall = handle_uninstall;
  iface->handle_update = handle_update;
}

static gboolean
eam_service_is_busy (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (priv->busy_counter > 0)
    return TRUE;

  return FALSE;
}


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
  GError *error = NULL;

  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (service),
                                    connection,
                                    "/com/endlessm/AppManager",
                                    &error);

  if (error) {
    eam_log_error_message ("Failed to register service object: %s\n", error->message);
    g_error_free (error);
  }
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
