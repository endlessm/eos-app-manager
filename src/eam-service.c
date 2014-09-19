/* Copyright 2014 Endless Mobile, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <polkit/polkit.h>

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "eam-config.h"
#include "eam-service.h"
#include "eam-updates.h"
#include "eam-refresh.h"
#include "eam-install.h"
#include "eam-uninstall.h"
#include "eam-list-avail.h"
#include "eam-dbus-utils.h"
#include "eam-version.h"

typedef struct _EamServicePrivate EamServicePrivate;

struct _EamServicePrivate {
  GDBusConnection *connection;
  guint registration_id;

  gchar *cfgfile;
  EamPkgdb *db;
  EamUpdates *updates;
  guint updates_id;
  guint filtered_id;
  EamTransaction *trans;
  GQueue *invocation_queue;

  gchar **available_updates;
  gboolean reloaddb;

  GTimer *timer;

  GCancellable *cancellable;

  PolkitAuthority *authority;
  gboolean authorizing;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamService, eam_service, G_TYPE_OBJECT)

G_DEFINE_QUARK (eam-service-error-quark, eam_service_error)

enum
{
  PROP_DB = 1,
  PROP_CFGFILE,
};

enum
{
  QUIT_REQUESTED,
  SIGNAL_MAX
};

static guint signals[SIGNAL_MAX];

typedef enum {
  EAM_SERVICE_METHOD_INSTALL,
  EAM_SERVICE_METHOD_UNINSTALL,
  EAM_SERVICE_METHOD_REFRESH,
  EAM_SERVICE_METHOD_LIST_AVAILABLE,
  EAM_SERVICE_METHOD_LIST_INSTALLED,
  EAM_SERVICE_METHOD_USER_CAPS,
  EAM_SERVICE_METHOD_CANCEL,
  EAM_SERVICE_METHOD_QUIT,
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

static void eam_service_install (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params);
static void eam_service_uninstall (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params);
static void eam_service_refresh (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params);
static void eam_service_list_avail (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params);
static void eam_service_list_installed (EamService *service,
  GDBusMethodInvocation *invocation, GVariant *params);
static void eam_service_get_user_caps (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params);
static void eam_service_quit (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params);
static void eam_service_cancel (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params);

static void run_method_with_authorization (EamService *service, GDBusMethodInvocation *invocation,
  EamServiceMethod method, GVariant *params);

#define AUTH_NAMESPACE "com.endlessm.app-installer."
#define AUTH_MSG "Authentication is required to "
#define AUTH_MSG_INSTALL AUTH_MSG "install or update software"
#define AUTH_MSG_UNINSTALL AUTH_MSG "uninstall software"
#define AUTH_MSG_REFRESH AUTH_MSG "refresh the list of available applications"
#define AUTH_MSG_CANCEL AUTH_MSG "cancel the application manager ongoing task"

static EamServiceAuth auth_action[] = {
  {EAM_SERVICE_METHOD_INSTALL,   "Install",       eam_service_install,    AUTH_NAMESPACE "install-application",   AUTH_MSG_INSTALL},
  {EAM_SERVICE_METHOD_UNINSTALL, "Uninstall",     eam_service_uninstall,  AUTH_NAMESPACE "uninstall-application", AUTH_MSG_UNINSTALL},
  {EAM_SERVICE_METHOD_REFRESH,   "Refresh",       eam_service_refresh,    NULL, ""},
  {EAM_SERVICE_METHOD_LIST_AVAILABLE, "ListAvailable", eam_service_list_avail, NULL, ""},
  {EAM_SERVICE_METHOD_LIST_INSTALLED, "ListInstalled", eam_service_list_installed, NULL, ""},
  {EAM_SERVICE_METHOD_USER_CAPS, "GetUserCapabilities", eam_service_get_user_caps, NULL, "" },
  {EAM_SERVICE_METHOD_CANCEL,    "Cancel",        eam_service_cancel,     AUTH_NAMESPACE "cancel-request",        AUTH_MSG_CANCEL},
  {EAM_SERVICE_METHOD_QUIT,      "Quit",          eam_service_quit,       NULL /* No auth required */,            ""},
};

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

  g_clear_pointer (&priv->cfgfile, g_free);
  g_clear_pointer (&priv->timer, g_timer_destroy);
  g_clear_pointer (&priv->available_updates, g_strfreev);
  g_clear_object (&priv->db);

  if (priv->updates && priv->updates_id) {
    g_signal_handler_disconnect (priv->updates, priv->updates_id);
    priv->updates_id = 0;
  }

  if (priv->updates && priv->filtered_id) {
    g_signal_handler_disconnect (priv->updates, priv->filtered_id);
    priv->filtered_id = 0;
  }

  if (priv->invocation_queue) {
    g_queue_free_full (priv->invocation_queue, (GDestroyNotify) eam_invocation_info_free);
    priv->invocation_queue = NULL;
  }

  g_clear_object (&priv->updates);
  g_clear_object (&priv->cancellable);
  g_clear_object (&priv->authority);

  G_OBJECT_CLASS (eam_service_parent_class)->dispose (obj);
}

static void
eam_service_set_property (GObject *obj, guint prop_id, const GValue *value,
  GParamSpec *pspec)
{
  EamServicePrivate *priv = eam_service_get_instance_private (EAM_SERVICE (obj));

  switch (prop_id) {
  case PROP_DB:
    priv->db = g_object_ref_sink (g_value_get_object (value));
    break;
  case PROP_CFGFILE:
    priv->cfgfile = g_value_dup_string (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
eam_service_class_init (EamServiceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = eam_service_dispose;
  object_class->set_property = eam_service_set_property;

  /**
   * EamService:db:
   *
   * The #EamPkdb to handle by this service
   */
  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "database", "", EAM_TYPE_PKGDB,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * EamDbusServer:cfgfile:
   *
   * The service configuration file. It can be %NULL for the default
   * config file.
   */
  g_object_class_install_property (object_class, PROP_CFGFILE,
    g_param_spec_string ("cfg-file", "config-file", "Configuration File", NULL,
      G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  /* signals */
  signals[QUIT_REQUESTED] = g_signal_new ("quit-requested",
    G_TYPE_FROM_CLASS(class), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
    g_cclosure_marshal_generic, G_TYPE_NONE, 0);
}

static void
eam_service_init (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  priv->cancellable = g_cancellable_new ();
  priv->reloaddb = TRUE; /* initial state */
  priv->timer = g_timer_new ();
  priv->invocation_queue = g_queue_new ();
}

/**
 * eam_service_new:
 * @db: a #EamPkgdb instance.
 *
 * Returns: Creates a new #EamService instance.
 **/
EamService *
eam_service_new (EamPkgdb *db)
{
  return g_object_new (EAM_TYPE_SERVICE, "db", db, NULL);
}

static void
append_pkg_list_to_variant_builder (GVariantBuilder *builder, const GList *list)
{
  const GList *l;
  for (l = list; l; l = l->next) {
    const EamPkg *pkg = l->data;
    gchar *version = eam_pkg_version_as_string (eam_pkg_get_version (pkg));
    g_variant_builder_add (builder, "(sss)", eam_pkg_get_id (pkg),
      eam_pkg_get_name (pkg), version);
    g_free (version);
  }
}

static GVariant *
build_avail_pkg_list_variant (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(a(sss)a(sss))"));

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a(sss)"));
  append_pkg_list_to_variant_builder (&builder, eam_updates_get_installables (priv->updates));
  g_variant_builder_close (&builder);

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a(sss)"));
  append_pkg_list_to_variant_builder (&builder, eam_updates_get_upgradables (priv->updates));
  g_variant_builder_close (&builder);

  return g_variant_builder_end (&builder);
}

static GVariant *
build_available_updates_variant (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (priv->available_updates)
    return g_variant_new_strv ((const gchar * const *) priv->available_updates, -1);

  GVariantBuilder empty;
  g_variant_builder_init (&empty, G_VARIANT_TYPE ("as"));
  return g_variant_new ("as", &empty);
}

/*
 * let's build a new available_updates string array and compare it
 * with the current one.
 *
 * If they are different, raise the PropertyChanged signal.
 *
 * Replace the string array afterwards.
 */
static void
updates_filtered_cb (EamService *service, gpointer data)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (!priv->connection)
    return;

  const GList *upgradables = eam_updates_get_upgradables (priv->updates);
  guint nlen = g_list_length ((GList *) upgradables); /* new length */
  guint olen = priv->available_updates ? g_strv_length (priv->available_updates)
    : 0; /* old length */

  if (nlen == 0 && olen == 0)
    return; /* odd case */

  gchar **available_updates = g_new0 (gchar *, nlen + 1);

  guint i = 0;
  const GList *l;
  for (l = upgradables; l; l = l->next) {
    const EamPkg *pkg = l->data;
    available_updates[i++] = g_strdup (eam_pkg_get_id (pkg));
  }

  if (nlen != olen)
    goto do_signal;

  /* don't you love O^2? */
  guint j, found = 0;
  for (i = 0; i < nlen; i++) {
    for (j = 0; j < olen; j++) {
      if (g_strcmp0 (available_updates[i], priv->available_updates[j]) == 0) {
        found++;
        break;
      }
    }
  }

  if (found != nlen)
    goto do_signal;

  g_clear_pointer (&available_updates, g_strfreev);
  return;

do_signal:
  g_clear_pointer (&priv->available_updates, g_strfreev);
  priv->available_updates = available_updates;

  {
    GVariantBuilder changed, invalidated;

    g_variant_builder_init (&invalidated, G_VARIANT_TYPE ("as"));
    g_variant_builder_init (&changed, G_VARIANT_TYPE_ARRAY);
    g_variant_builder_add (&changed, "{sv}", "AvailableUpdates",
      build_available_updates_variant (service));

    GVariant *params = g_variant_new ("(sa{sv}as)", "com.endlessm.AppManager",
      &changed, &invalidated);

    GError *error = NULL;
    g_dbus_connection_emit_signal (priv->connection, NULL,
      "/com/endlessm/AppManager", "org.freedesktop.DBus.Properties",
      "PropertiesChanged", params, &error);

    if (error) {
      g_critical ("Couldn't emit DBus signal \"PropertiesChanged\": %s",
        error->message);
      g_clear_error (&error);
    }
  }
}

static void
avails_changed_cb (EamService *service, gpointer data)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (!priv->connection)
    return;

  GError *error = NULL;
  g_dbus_connection_emit_signal (priv->connection, NULL,
    "/com/endlessm/AppManager", "com.endlessm.AppManager",
    "AvailableApplicationsChanged", build_avail_pkg_list_variant (service),
    &error);

  if (error) {
    g_critical ("Couldn't emit DBus signal \"AvailableApplicationsChanged\": %s",
      error->message);
    g_clear_error (&error);
  }
}

static EamUpdates *
get_eam_updates (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (!priv->updates) {
    priv->updates = eam_updates_new ();

    /* let's read what we have, without refreshing the database and
     * ignoring parsing errors. */
    eam_updates_parse (priv->updates, NULL);

    /* we connect after parse because we want to go silence at
     * first. */
    priv->updates_id = g_signal_connect_swapped (priv->updates,
      "available-apps-changed", G_CALLBACK (avails_changed_cb), service);

    priv->filtered_id = g_signal_connect_swapped (priv->updates,
      "updates-filtered", G_CALLBACK (updates_filtered_cb), service);
  }

  return priv->updates;
}

static void
eam_service_set_reloaddb (EamService *service, gboolean value)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (priv->reloaddb && !value)
    {
      EamUpdates *updates = get_eam_updates (service);
      eam_updates_parse (updates, NULL);
      eam_updates_filter (updates, priv->db, NULL);
    }

  priv->reloaddb = value;
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
eam_service_clear_transaction (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);
  g_assert (priv->trans);

  g_cancellable_reset (priv->cancellable);
  g_clear_object (&priv->trans); /* we don't need you anymore */

  eam_service_check_queue (service);
}

static void
run_eam_transaction (EamService *service, GDBusMethodInvocation *invocation,
  GAsyncReadyCallback callback)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);
  g_assert (priv->trans);

  g_object_set_data (G_OBJECT (priv->trans), "invocation", invocation);
  eam_transaction_run_async (priv->trans, priv->cancellable, callback, service);
}

typedef void (*EamRunService) (EamService *service, const gchar *appid,
  GDBusMethodInvocation *invocation);

struct _load_pkgdb_clos {
  EamService *service;
  GDBusMethodInvocation *invocation;
  GAsyncReadyCallback callback;

  gchar *appid;
  EamRunService run_service;
};

static void
load_pkgdb_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  struct _load_pkgdb_clos *clos = data;
  EamServicePrivate *priv = eam_service_get_instance_private (clos->service);

  if (GPOINTER_TO_INT (priv->trans) == 1)
    priv->trans = NULL; /* clear the dummy transaction */

  GError *error = NULL;
  eam_pkgdb_load_finish (EAM_PKGDB (source), res, &error);
  if (error) {
    g_dbus_method_invocation_take_error (clos->invocation, error);
    eam_service_clear_transaction (clos->service);
    goto out;
  }

  eam_service_set_reloaddb (clos->service, FALSE);

  if (clos->run_service)
    clos->run_service (clos->service, clos->appid, clos->invocation);
  else
    run_eam_transaction (clos->service, clos->invocation, clos->callback);

out:
  g_free (clos->appid);
  g_slice_free (struct _load_pkgdb_clos, clos);
}

static void
run_eam_service_with_load_pkgdb (EamService *service, const gchar *appid,
  EamRunService run_service, GDBusMethodInvocation *invocation)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (priv->reloaddb) {
    struct _load_pkgdb_clos *clos = g_slice_new0 (struct _load_pkgdb_clos);
    clos->service = service;
    clos->invocation = invocation;
    clos->appid = g_strdup (appid);
    clos->run_service = run_service;

    priv->trans = GINT_TO_POINTER (1); /* let's say we're running a transaction */

    eam_pkgdb_load_async (priv->db, priv->cancellable, load_pkgdb_cb, clos);
    return;
  }

  if (run_service)
    run_service (service, appid, invocation);
}

static void
run_eam_transaction_with_load_pkgdb (EamService *service, GDBusMethodInvocation *invocation,
  GAsyncReadyCallback callback)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (priv->reloaddb) {
    struct _load_pkgdb_clos *clos = g_slice_new0 (struct _load_pkgdb_clos);
    clos->service = service;
    clos->invocation = invocation;
    clos->callback = callback;

    eam_pkgdb_load_async (priv->db, priv->cancellable, load_pkgdb_cb, clos);
    return;
  }

  run_eam_transaction (service, invocation, callback);
}

static void
refresh_cb (GObject *source, GAsyncResult *res, gpointer data)
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
  eam_service_clear_transaction (service);
}

static void
eam_service_reset_timer (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  g_timer_reset (priv->timer);
}

static void
eam_service_refresh (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  /* This is kind of a HACK for a specific use-case:
   *
   * When the user changes the server and the eam process is still
   * alive, the user wants to get the bundle list from the new server.
   *
   * Our solution (the simpler and inefficient) is to reload the
   * configuration file every time the reload method is called, since
   * the parsing and loading is relatively cheap.
   */
  eam_config_load_with_filename (NULL, priv->cfgfile);

  priv->trans = eam_refresh_new (priv->db, get_eam_updates (service));
  run_eam_transaction_with_load_pkgdb (service, invocation, refresh_cb);
}

static void
reload_pkgdb_after_transaction_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  EamService *service = EAM_SERVICE (data);
  EamServicePrivate *priv = eam_service_get_instance_private (service);
  GDBusMethodInvocation *invocation = g_object_get_data (G_OBJECT (priv->trans),
    "invocation");
  g_assert (invocation);
  g_object_set_data (G_OBJECT (priv->trans), "invocation", NULL);

  GError *error = NULL;
  eam_pkgdb_load_finish (EAM_PKGDB (source), res, &error);
  if (error) {
    g_dbus_method_invocation_take_error (invocation, error);
    goto out;
  }

  eam_service_set_reloaddb (service, FALSE);

  GVariant *value = g_variant_new ("(b)", TRUE);
  g_dbus_method_invocation_return_value (invocation, value);

  /* Let's notify the available apps list has changed, as an installed app is
     not available anymore, and uninstalled app becomes available */
  avails_changed_cb (service, NULL);

out:
  eam_service_clear_transaction (service);
}

static void
install_or_uninstall_cb (GObject *source, GAsyncResult *res, gpointer data)
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

  if (ret) {
    /* if we installed, uninstalled or updated something we reload the
     * database */
    eam_service_set_reloaddb (service, TRUE);
    eam_pkgdb_load_async (priv->db, priv->cancellable,
      reload_pkgdb_after_transaction_cb, service);
    return;
  }

  g_object_set_data (source, "invocation", NULL);
  GVariant *value = g_variant_new ("(b)", ret);
  g_dbus_method_invocation_return_value (invocation, value);

out:
  eam_service_clear_transaction (service);
}

static void
run_service_install (EamService *service, const gchar *appid,
  GDBusMethodInvocation *invocation)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  eam_service_reset_timer (service);

  if (eam_updates_pkg_is_upgradable (get_eam_updates (service), appid)) {
    /* update from the current version to the last version */
    const EamPkg *pkg = eam_pkgdb_get (priv->db, appid);
    g_assert (pkg);

    EamPkgVersion *pkg_version = eam_pkg_get_version (pkg);
    gchar *from_version = eam_pkg_version_as_string (pkg_version);

    priv->trans = eam_install_new_from_version (appid, from_version);
    g_free (from_version);

    run_eam_transaction (service, invocation, install_or_uninstall_cb);
  } else if (eam_updates_pkg_is_installable (get_eam_updates (service), appid)) {
    /* install the latest version */
    priv->trans = eam_install_new (appid);
    run_eam_transaction (service, invocation, install_or_uninstall_cb);
  } else {
    g_dbus_method_invocation_return_error (invocation, EAM_SERVICE_ERROR,
      EAM_SERVICE_ERROR_PKG_UNKNOWN, _("Application '%s' is unknown"),
      appid);
  }
}

static void
eam_service_install (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params)
{
  const gchar *appid = NULL;
  g_variant_get (params, "(&s)", &appid);

  run_eam_service_with_load_pkgdb (service, appid, run_service_install,
    invocation);
}

static void
run_service_uninstall (EamService *service, const gchar *appid,
  GDBusMethodInvocation *invocation)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (!eam_pkgdb_get (priv->db, appid)) {
    g_dbus_method_invocation_return_error (invocation, EAM_SERVICE_ERROR,
      EAM_SERVICE_ERROR_PKG_UNKNOWN, _("Application '%s' is not installed"),
      appid);
  } else {
    priv->trans = eam_uninstall_new (appid);
    run_eam_transaction (service, invocation, install_or_uninstall_cb);
  }
}

static void
eam_service_uninstall (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params)
{
  const gchar *appid = NULL;
  g_variant_get (params, "(&s)", &appid);

  run_eam_service_with_load_pkgdb (service, appid, run_service_uninstall,
    invocation);
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
    g_warning ("Unable to check authorisation for %s: %s", action,
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
  gboolean can_refresh = FALSE;

  /* XXX: we want to have a separate configuration to decide the capabilities
   * for each user, but for the time being we can use the 'wheel' group to
   * decide if a user has the capabilities to install/update/remove apps.
   */
  if (user_is_in_admin_group (user, EAM_ADMIN_GROUP_NAME)) {
    can_install = TRUE;
    can_uninstall = TRUE;
    can_refresh = TRUE;
    goto out;
  }

  /* if the user is not in the ADMIN_GROUP_NAME then we go through
   * a polkit check
   */
  PolkitSubject *subject = polkit_system_bus_name_new (sender);
  if (!subject) {
    g_warning ("Unable to create the Polkit subject for: %s", sender);
    goto out;
  }

  can_install = eam_service_check_auth_by_method (service, subject,
    EAM_SERVICE_METHOD_INSTALL);
  can_uninstall = eam_service_check_auth_by_method (service, subject,
    EAM_SERVICE_METHOD_UNINSTALL);
  can_refresh = TRUE; 

out:
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(a{sv})"));
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));

  g_variant_builder_add (&builder, "{sv}", "CanInstall", g_variant_new_boolean (can_install));
  g_variant_builder_add (&builder, "{sv}", "CanUninstall", g_variant_new_boolean (can_uninstall));
  g_variant_builder_add (&builder, "{sv}", "CanRefresh", g_variant_new_boolean (can_refresh));

  g_variant_builder_close (&builder);
  GVariant *res = g_variant_builder_end (&builder);
  g_dbus_method_invocation_return_value (invocation, res);
}

static void
list_avail_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  GDBusMethodInvocation *invocation = g_object_get_data (source, "invocation");
  g_assert (invocation);
  g_object_set_data (source, "invocation", NULL);

  EamService *service = EAM_SERVICE (data);
  EamServicePrivate *priv = eam_service_get_instance_private (service);
  g_assert (source == G_OBJECT (priv->trans));

  eam_transaction_finish (priv->trans, res, NULL);

  g_dbus_method_invocation_return_value (invocation, build_avail_pkg_list_variant (service));

  eam_service_clear_transaction (service);
}

static void
eam_service_list_avail (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  GVariant *opts = g_variant_get_child_value (params, 0);

  const char *language = NULL;

  g_variant_lookup (opts, "Locale", "&s", &language);

  priv->trans = eam_list_avail_new (priv->reloaddb, priv->db, get_eam_updates (service), language);
  run_eam_transaction_with_load_pkgdb (service, invocation, list_avail_cb);
}

static void
build_list_installed (EamService *service, const gchar *appid,
  GDBusMethodInvocation *invocation)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);
  GVariantBuilder builder;
  EamPkg *pkg = NULL;

  eam_service_reset_timer (service);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(a(sss))"));
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a(sss)"));

  while (eam_pkgdb_iter_next (priv->db, &pkg)) {
    if (!pkg)
      continue;

    gchar *version = eam_pkg_version_as_string (eam_pkg_get_version (pkg));
    g_variant_builder_add (&builder, "(sss)", eam_pkg_get_id (pkg),
      eam_pkg_get_name (pkg), version);
    g_free (version);
  }
  eam_pkgdb_iter_reset (priv->db);

  g_variant_builder_close (&builder);
  g_dbus_method_invocation_return_value (invocation,
    g_variant_builder_end (&builder));
}

static void
eam_service_list_installed (EamService *service,
  GDBusMethodInvocation *invocation, GVariant *params)
{
  run_eam_service_with_load_pkgdb (service, NULL, build_list_installed,
    invocation);
}

static void
eam_service_quit (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params)
{
  g_signal_emit (service, signals[QUIT_REQUESTED], 0);
  g_dbus_method_invocation_return_value (invocation, NULL);
}

static void
eam_service_cancel (EamService *service, GDBusMethodInvocation *invocation,
  GVariant *params)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (!priv->trans || priv->authorizing) /* are we not running a transaction
                                            or are we authorizing the user? */
    goto bail;

  GError *error = NULL;
  if (g_cancellable_set_error_if_cancelled (priv->cancellable, &error)) {
    g_dbus_method_invocation_take_error (invocation, error);
    return;
  }

  g_cancellable_cancel (priv->cancellable);

bail:
  g_dbus_method_invocation_return_value (invocation, NULL);
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
    g_dbus_method_invocation_return_error (info->invocation, EAM_SERVICE_ERROR,
      EAM_SERVICE_ERROR_NOT_AUTHORIZED, _("Not authorized to perform the operation"));
    goto bail;
  }

  if (auth_action[info->method].run)
    auth_action[info->method].run (info->service, info->invocation, info->params);

bail:
  eam_invocation_info_free (info);
  if (result)
    g_object_unref (result);
}

static gboolean
eam_service_check_authorization_async (EamService *service, GDBusMethodInvocation *invocation,
  EamServiceMethod method, GVariant *params)
{
  PolkitDetails *details;

  if (!auth_action[method].action_id) {
    /* The service does not any require authorization */
    auth_action[method].run (service, invocation, params);
    return TRUE;
  }

  const gchar *sender = g_dbus_method_invocation_get_sender (invocation);

  PolkitSubject *subject = polkit_system_bus_name_new (sender);
  if (subject == NULL) {
    g_warning ("Unable to create the Polkit subject for: %s", sender);

    return FALSE;
  }

  EamServicePrivate *priv = eam_service_get_instance_private (service);
  priv->authorizing = TRUE;

  details = polkit_details_new ();
  polkit_details_insert (details, "polkit.message", N_(auth_action[method].message));
  polkit_details_insert (details, "polkit.gettext_domain", GETTEXT_PACKAGE);

  EamInvocationInfo *info = eam_invocation_info_new (service, invocation, method, params);
  polkit_authority_check_authorization (priv->authority, subject, auth_action[method].action_id,
    details, POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION, priv->cancellable,
    (GAsyncReadyCallback) check_authorization_cb, info);

  g_object_unref (subject);
  g_object_unref (details);

  return TRUE;
}

static void
run_method_with_authorization (EamService *service, GDBusMethodInvocation *invocation,
			       EamServiceMethod method, GVariant *params)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  /* Run the operation only if the user is authorized to perform the action */
  if (!eam_service_check_authorization_async (service, invocation, method, params)) {
    g_dbus_method_invocation_return_error (invocation, EAM_SERVICE_ERROR,
      EAM_SERVICE_ERROR_AUTHORIZATION, _("An error happened during the authorization process"));
    priv->authorizing = FALSE;
  }
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

  if (eam_service_is_busy (service) && method != EAM_SERVICE_METHOD_CANCEL) {
    EamInvocationInfo *info = eam_invocation_info_new (service, invocation, method, params);
    g_queue_push_tail (priv->invocation_queue, info);

    return;
  }

  run_method_with_authorization (service, invocation, method, params);
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

  for (method_i = 0; method_i < G_N_ELEMENTS (auth_action); method_i++)
    if (g_strcmp0 (method, auth_action[method_i].dbus_name) == 0) {
      eam_service_run (service, invocation, method_i, params);
      break;
    }
}

static GVariant *
handle_get_property (GDBusConnection *connection, const gchar *sender,
  const gchar *path, const gchar *interface, const gchar *name, GError **error,
  gpointer data)
{
  EamService *service = EAM_SERVICE (data);

  eam_service_reset_timer (service);

  if (g_strcmp0 (interface, "com.endlessm.AppManager"))
    return NULL;

  if (!g_strcmp0 (name, "AvailableUpdates"))
    return build_available_updates_variant (service);

  /* return an error */
  g_set_error (error, EAM_SERVICE_ERROR, EAM_SERVICE_ERROR_UNIMPLEMENTED,
    "Property '%s' is not implemented", name);

  return NULL;
}

static const GDBusInterfaceVTable interface_vtable = {
  handle_method_call, handle_get_property, NULL,
};

static GDBusNodeInfo*
load_introspection (GError **error)
{
  GDBusNodeInfo *info = NULL;
  GBytes *data = g_resources_lookup_data ("/com/endlessm/AppManager/eam-dbus-interface.xml",
    G_RESOURCE_LOOKUP_FLAGS_NONE, error);

  if (!data)
    return NULL;

  info = g_dbus_node_info_new_for_xml (g_bytes_get_data (data, NULL), error);
  g_bytes_unref (data);
  return info;
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
  static GDBusNodeInfo *introspection = NULL;
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  g_return_if_fail (EAM_IS_SERVICE (service));
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));

  if (!introspection)
    introspection = load_introspection (&error);

  if (error) {
    g_warning ("Failed to load DBus introspection: %s", error->message);
    g_error_free (error);
    return;
  }

  priv->registration_id = g_dbus_connection_register_object (connection,
    "/com/endlessm/AppManager", introspection->interfaces[0], &interface_vtable,
    service, NULL, &error);

  if (!priv->registration_id) {
    g_warning ("Failed to register service object: %s\n", error->message);
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

  if (eam_service_is_busy (service))
      return 0;

  return g_timer_elapsed (priv->timer, NULL);
}
