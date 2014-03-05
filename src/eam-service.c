/* Copyright 2014 Endless Mobile, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "eam-service.h"

typedef struct _EamServicePrivate EamServicePrivate;

struct _EamServicePrivate {
  GDBusConnection *connection;
  guint registration_id;

  EamPkgdb *db;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamService, eam_service, G_TYPE_OBJECT)

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

  if (priv->db)
    g_clear_object (&priv->db);

  G_OBJECT_CLASS (eam_service_parent_class)->dispose (obj);
}

static void
eam_service_set_property (GObject *obj, guint prop_id, const GValue *value,
  GParamSpec *pspec)
{
  switch (prop_id) {
  case PROP_DB:
    eam_service_initialize (EAM_SERVICE (obj), g_value_get_object (value));
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
      G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
}

static void
eam_service_init (EamService *service)
{
}

static gpointer
eam_service_create_instance (gpointer data)
{
  return g_object_new (EAM_TYPE_SERVICE, NULL);
}

EamService *
eam_service_get (void)
{
  static GOnce once_init = G_ONCE_INIT;
  return EAM_SERVICE (g_once (&once_init, eam_service_create_instance, NULL));
}

static void
eam_service_refresh (EamService *service)
{
  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (!priv->db)
    return;

  eam_pkgdb_load (priv->db);
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
    eam_service_refresh (service);
    g_dbus_method_invocation_return_value (invocation, NULL);
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

void
eam_service_initialize (EamService *service, EamPkgdb *db)
{
  g_return_if_fail (EAM_IS_SERVICE (service));
  g_return_if_fail (EAM_IS_PKGDB (db));

  EamServicePrivate *priv = eam_service_get_instance_private (service);

  if (priv->db) {
    g_warning ("Don't set the package database twice.");
    return;
  }

  priv->db = g_object_ref_sink (db);
  eam_pkgdb_load (priv->db);
}
