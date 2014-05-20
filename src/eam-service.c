/* Copyright 2014 Endless Mobile, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>

#include "eam-service.h"
#include "eam-updates.h"
#include "eam-refresh.h"
#include "eam-install.h"
#include "eam-uninstall.h"
#include "eam-update.h"
#include "eam-list-avail.h"

typedef struct _EamServicePrivate EamServicePrivate;

struct _EamServicePrivate {
  GDBusConnection *connection;
  guint registration_id;

  EamPkgdb *db;
  EamUpdates *updates;
  guint updates_id;
  guint filtered_id;
  EamTransaction *trans;

  gchar **available_updates;
  gboolean reloaddb;

  GTimer *timer;

  GCancellable *cancellable;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamService, eam_service, G_TYPE_OBJECT)

G_DEFINE_QUARK (eam-service-error-quark, eam_service_error)

enum
{
  PROP_DB = 1,
};

enum
{
  QUIT_REQUESTED,
  SIGNAL_MAX
};

static guint signals[SIGNAL_MAX];

static void
eam_service_dispose (GObject *obj)
{
  EamServicePrivate *priv = eam_service_get_instance_private (EAM_SERVICE (obj));

  if (priv->connection) {
    g_dbus_connection_unregister_object (priv->connection, priv->registration_id);
    priv->registration_id = 0;
    priv->connection = NULL;
  }

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
  g_clear_object (&priv->updates);

  g_clear_object (&priv->cancellable);

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

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sss)"));

  append_pkg_list_to_variant_builder (&builder,
    eam_updates_get_installables (priv->updates));
  append_pkg_list_to_variant_builder (&builder,
    eam_updates_get_upgradables (priv->updates));

  GVariant *value = g_variant_builder_end (&builder);
  return g_variant_new_tuple (&value, 1);
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

    GVariant *params = g_variant_new ("(sa{sv}as)", "com.Endless.AppManager",
      &changed, &invalidated);

    GError *error = NULL;
    g_dbus_connection_emit_signal (priv->connection, NULL,
      "/com/Endless/AppManager", "org.freedesktop.DBus.Properties",
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
    "/com/Endless/AppManager", "com.Endless.AppManager",
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
    eam_updates_filter (get_eam_updates (service), priv->db);

  priv->reloaddb = value;
}

static void
eam_service_clear_transaction (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);
  g_assert (priv->trans);

  g_cancellable_reset (priv->cancellable);
  g_clear_object (&priv->trans); /* we don't need you anymore */
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

struct _load_pkgdb_clos {
  EamService *service;
  GDBusMethodInvocation *invocation;
  GAsyncReadyCallback callback;

  gchar *appid;
  void (*run_service) (EamService*, const gchar*, GDBusMethodInvocation*);
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
eam_service_refresh (EamService *service, GDBusMethodInvocation *invocation)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  eam_service_reset_timer (service);

  if (priv->trans) { /* are we running a transaction? */
    g_dbus_method_invocation_return_error (invocation, EAM_SERVICE_ERROR,
      EAM_SERVICE_ERROR_BUSY, _("Service is busy with a previous task"));
    return;
  }

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
    /* update to the last version */
    priv->trans = eam_update_new (appid);
    run_eam_transaction (service, invocation, install_or_uninstall_cb);
  } else if (eam_updates_pkg_is_installable (get_eam_updates (service), appid)) {
    /* install the latest version (which is NULL) */
    priv->trans = eam_install_new (appid, NULL);
    run_eam_transaction (service, invocation, install_or_uninstall_cb);
  } else {
    g_dbus_method_invocation_return_error (invocation, EAM_SERVICE_ERROR,
      EAM_SERVICE_ERROR_PKG_UNKNOWN, _("Application '%s' is unknown"),
      appid);
  }
}

static void
eam_service_install (EamService *service, const gchar *appid,
  GDBusMethodInvocation *invocation)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  eam_service_reset_timer (service);

  if (priv->trans) { /* are we running a transaction? */
    g_dbus_method_invocation_return_error (invocation, EAM_SERVICE_ERROR,
      EAM_SERVICE_ERROR_BUSY, _("Service is busy with a previous task"));
    return;
  }

  if (priv->reloaddb) {
    struct _load_pkgdb_clos *clos = g_slice_new0 (struct _load_pkgdb_clos);
    clos->service = service;
    clos->invocation = invocation;
    clos->appid = g_strdup (appid);
    clos->run_service = run_service_install;

    priv->trans = GINT_TO_POINTER (1); /* let's say we're running a transaction */

    eam_pkgdb_load_async (priv->db, priv->cancellable, load_pkgdb_cb, clos);
    return;
  }

  run_service_install (service, appid, invocation);
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
eam_service_uninstall (EamService *service, const gchar *appid,
  GDBusMethodInvocation *invocation)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  eam_service_reset_timer (service);

  if (priv->trans) { /* are we running a transaction? */
    g_dbus_method_invocation_return_error (invocation, EAM_SERVICE_ERROR,
      EAM_SERVICE_ERROR_BUSY, _("Service is busy with a previous task"));
    return;
  }

  if (priv->reloaddb) {
    struct _load_pkgdb_clos *clos = g_slice_new0 (struct _load_pkgdb_clos);
    clos->service = service;
    clos->invocation = invocation;
    clos->appid = g_strdup (appid);
    clos->run_service = run_service_uninstall;

    priv->trans = GINT_TO_POINTER (1); /* let's say we're running a transaction */

    eam_pkgdb_load_async (priv->db, priv->cancellable, load_pkgdb_cb, clos);
    return;
  }

  run_service_uninstall (service, appid, invocation);
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

  g_dbus_method_invocation_return_value (invocation,
    build_avail_pkg_list_variant (service));

  eam_service_clear_transaction (service);
}

static void
eam_service_list_avail (EamService *service, GDBusMethodInvocation *invocation)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  eam_service_reset_timer (service);

  if (priv->trans) { /* are we running a transaction? */
    g_dbus_method_invocation_return_error (invocation, EAM_SERVICE_ERROR,
      EAM_SERVICE_ERROR_BUSY, _("Service is busy with a previous task"));
    return;
  }

  priv->trans = eam_list_avail_new (priv->reloaddb, priv->db,
    get_eam_updates (service));
  run_eam_transaction_with_load_pkgdb (service, invocation, list_avail_cb);
}

static void
eam_service_quit (EamService *service, GDBusMethodInvocation *invocation)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (priv->trans) { /* are we running a transaction? */
    g_dbus_method_invocation_return_error (invocation, EAM_SERVICE_ERROR,
      EAM_SERVICE_ERROR_BUSY, _("Service is busy with a previous task"));
    return;
  }

  g_signal_emit (service, signals[QUIT_REQUESTED], 0);
  g_dbus_method_invocation_return_value (invocation, NULL);
}

static void
eam_service_cancel (EamService *service, GDBusMethodInvocation *invocation)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (!priv->trans) /* are we running a transaction? */
    goto bail;

  GError *error = NULL;
  if (g_cancellable_set_error_if_cancelled (priv->cancellable, &error)) {
    g_dbus_method_invocation_return_gerror (invocation, error);
    g_clear_error (&error);
    return;
  }

  g_cancellable_cancel (priv->cancellable);

bail:
  g_dbus_method_invocation_return_value (invocation, NULL);
}

static void
handle_method_call (GDBusConnection *connection, const char *sender,
  const char *path, const char *interface, const char *method, GVariant *params,
  GDBusMethodInvocation *invocation, gpointer data)
{
  EamService *service = EAM_SERVICE (data);

  if (g_strcmp0 (interface, "com.Endless.AppManager"))
    return;

  if (!g_strcmp0 (method, "Refresh")) {
    eam_service_refresh (service, invocation);
  } else if (!g_strcmp0 (method, "Install")) {
    const gchar *appid = NULL;
    g_variant_get (params, "(&s)", &appid);
    eam_service_install (service, appid, invocation);
  } else if (!g_strcmp0 (method, "Uninstall")) {
    const gchar *appid = NULL;
    g_variant_get (params, "(&s)", &appid);
    eam_service_uninstall (service, appid, invocation);
  } else if (!g_strcmp0 (method, "ListAvailable")) {
    eam_service_list_avail (service, invocation);
  } else if (!g_strcmp0 (method, "Cancel")) {
    eam_service_cancel (service, invocation);
  } else if (!g_strcmp0 (method, "Quit")) {
    eam_service_quit (service, invocation);
  }
}

static GVariant *
handle_get_property (GDBusConnection *connection, const gchar *sender,
  const gchar *path, const gchar *interface, const gchar *name, GError **error,
  gpointer data)
{
  EamService *service = EAM_SERVICE (data);

  eam_service_reset_timer (service);

  if (g_strcmp0 (interface, "com.Endless.AppManager"))
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
  GBytes *data = g_resources_lookup_data ("/com/Endless/AppManager/eam-dbus-interface.xml",
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
    "/com/Endless/AppManager", introspection->interfaces[0], &interface_vtable,
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
 * eam_service_get_idle:
 * @service: a #EamService instance
 *
 * Returns: the idle elapsed time in seconds
 **/
guint
eam_service_get_idle (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  return g_timer_elapsed (priv->timer, NULL);
}
