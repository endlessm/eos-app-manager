/* Copyright 2014 Endless Mobile, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>

#include "eam-service.h"
#include "eam-updates.h"
#include "eam-refresh.h"
#include "eam-install.h"
#include "eam-list-avail.h"

typedef struct _EamServicePrivate EamServicePrivate;

struct _EamServicePrivate {
  GDBusConnection *connection;
  guint registration_id;

  EamPkgdb *db;
  EamUpdates *updates;
  EamTransaction *trans;

  gboolean reloaddb;

  GCancellable *cancellable;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamService, eam_service, G_TYPE_OBJECT)

G_DEFINE_QUARK (eam-service-error-quark, eam_service_error)

enum
{
  PROP_DB = 1,
};

static void
eam_service_dispose (GObject *obj)
{
  EamServicePrivate *priv = eam_service_get_instance_private (EAM_SERVICE (obj));

  if (priv->connection) {
    g_dbus_connection_unregister_object (priv->connection, priv->registration_id);
    priv->registration_id = 0;
    priv->connection = NULL;
  }

  g_clear_object (&priv->db);
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
}

static void
eam_service_init (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);
  priv->cancellable = g_cancellable_new ();
  priv->reloaddb = TRUE; /* initial state */
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

static EamUpdates *
get_eam_updates (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);
  if (!priv->updates) {
    priv->updates = eam_updates_new ();
    /* let's read what we have, without refreshing the database */
    if (eam_updates_parse (priv->updates, NULL) && priv->db)
      eam_updates_filter (priv->updates, priv->db);
  }

  return priv->updates;
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
};

static void
load_pkgdb_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  struct _load_pkgdb_clos *clos = data;
  EamServicePrivate *priv = eam_service_get_instance_private (clos->service);

  GError *error = NULL;
  eam_pkgdb_load_finish (EAM_PKGDB (source), res, &error);
  if (error) {
    g_dbus_method_invocation_take_error (clos->invocation, error);
    g_clear_object (&priv->trans);
    goto out;
  }

  priv->reloaddb = FALSE;
  run_eam_transaction (clos->service, clos->invocation, clos->callback);

out:
  g_slice_free (struct _load_pkgdb_clos, clos);
}

static void
run_eam_transaction_with_load_pkgdb (EamService *service, GDBusMethodInvocation *invocation,
  GAsyncReadyCallback callback)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (priv->reloaddb) {
    struct _load_pkgdb_clos *clos = g_slice_new (struct _load_pkgdb_clos);
    clos->service = service;
    clos->invocation = invocation;
    clos->callback = callback;

    eam_pkgdb_load_async (priv->db, priv->cancellable, load_pkgdb_cb, clos);
    return;
  }

  run_eam_transaction (service, invocation, callback);
}

static void
end_install_cb (GObject *source, GAsyncResult *res, gpointer data)
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

  priv->reloaddb = FALSE;

  GVariant *value = g_variant_new ("(b)", TRUE);
  g_dbus_method_invocation_return_value (invocation, value);

out:
  g_clear_object (&priv->trans);
}

static void
refresh_or_install_cb (GObject *source, GAsyncResult *res, gpointer data)
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

  if (ret && EAM_IS_INSTALL (priv->trans)) {
      priv->reloaddb = TRUE; /* if we installed something we reload the database */
      eam_pkgdb_load_async (priv->db, priv->cancellable, end_install_cb, service);
      return;
  }

  g_object_set_data (source, "invocation", NULL);
  GVariant *value = g_variant_new ("(b)", ret);
  g_dbus_method_invocation_return_value (invocation, value);

out:
  g_clear_object (&priv->trans); /* we don't need you anymore */
}

static void
eam_service_refresh (EamService *service, GDBusMethodInvocation *invocation)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (priv->trans) { /* are we running a transaction? */
    g_dbus_method_invocation_return_error (invocation, EAM_SERVICE_ERROR,
      EAM_SERVICE_ERROR_BUSY, _("Service is busy with a previous task"));
    return;
  }

  priv->trans = eam_refresh_new (priv->db, get_eam_updates (service));
  run_eam_transaction_with_load_pkgdb (service, invocation, refresh_or_install_cb);
}

static void
run_service_install (EamService *service, const gchar *appid,
  GDBusMethodInvocation *invocation)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (eam_pkgdb_get (priv->db, appid)) {
    /* update */
    g_dbus_method_invocation_return_error (invocation, EAM_SERVICE_ERROR,
      EAM_SERVICE_ERROR_UNIMPLEMENTED, _("Method '%s' not implemented yet"),
      "Update");
  } else if (eam_updates_pkg_is_installable (get_eam_updates (service), appid)) {
    /* install the latest version (which is NULL) */
    priv->trans = eam_install_new (appid, NULL);
    g_object_set_data (G_OBJECT (priv->trans), "invocation", invocation);
    eam_transaction_run_async (priv->trans, priv->cancellable,
      refresh_or_install_cb, service);
  } else {
    g_dbus_method_invocation_return_error (invocation, EAM_SERVICE_ERROR,
      EAM_SERVICE_ERROR_PKG_UNKNOWN, _("Application '%s' is unknown"),
      appid);
  }
}

struct _load_pkgdb_install_clos {
  EamService *service;
  GDBusMethodInvocation *invocation;
  gchar *appid;
};

static void
load_pkgdb_install_cb (GObject *source, GAsyncResult *res, gpointer data)
{
  struct _load_pkgdb_install_clos *clos = data;
  EamServicePrivate *priv = eam_service_get_instance_private (clos->service);

  GError *error = NULL;
  eam_pkgdb_load_finish (EAM_PKGDB (source), res, &error);
  if (error) {
    priv->trans = NULL; /* clear the dummy transaction */
    g_dbus_method_invocation_take_error (clos->invocation, error);
    goto out;
  }

  priv->reloaddb = FALSE;
  run_service_install (clos->service, clos->appid, clos->invocation);

out:
  g_free (clos->appid);
  g_slice_free (struct _load_pkgdb_install_clos, clos);
}

static void
eam_service_install (EamService *service, const gchar *appid,
  GDBusMethodInvocation *invocation)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (priv->trans) { /* are we running a transaction? */
    g_dbus_method_invocation_return_error (invocation, EAM_SERVICE_ERROR,
      EAM_SERVICE_ERROR_BUSY, _("Service is busy with a previous task"));
    return;
  }

  if (priv->reloaddb) {
    struct _load_pkgdb_install_clos *clos = g_slice_new (struct _load_pkgdb_install_clos);
    clos->service = service;
    clos->invocation = invocation;
    clos->appid = g_strdup (appid);

    priv->trans = GINT_TO_POINTER (1); /* let's say we're running a transaction */

    eam_pkgdb_load_async (priv->db, priv->cancellable, load_pkgdb_install_cb,
      clos);
    return;
  }

  run_service_install (service, appid, invocation);
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

  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sss)"));
  append_pkg_list_to_variant_builder (&builder,
    eam_updates_get_installables (priv->updates));
  append_pkg_list_to_variant_builder (&builder,
    eam_updates_get_upgradables (priv->updates));
  GVariant *value = g_variant_builder_end (&builder);
  GVariant *tuple = g_variant_new_tuple (&value, 1);
  g_dbus_method_invocation_return_value (invocation, tuple);

  g_clear_object (&priv->trans); /* we don't need you anymore */
}

static void
eam_service_list_avail (EamService *service, GDBusMethodInvocation *invocation)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

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
  } else if (!g_strcmp0 (method, "ListAvailable")) {
    eam_service_list_avail (service, invocation);
  }
}

static const GDBusInterfaceVTable interface_vtable = {
  handle_method_call, NULL, NULL,
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
