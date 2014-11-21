/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include <gio/gio.h>
#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif

#include "eam-dbus-server.h"
#include "eam-service.h"
#include "eam-config.h"
#include "eam-log.h"

typedef struct _EamDbusServerPrivate EamDbusServerPrivate;

struct _EamDbusServerPrivate {
  GMainLoop *mainloop;
  guint busowner;
  guint hangup;
  guint terminate;
  guint interrupt;

  guint quit_id;
  guint timer_id;
  EamService *service;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamDbusServer, eam_dbus_server, G_TYPE_OBJECT)

enum
{
  PROP_DB = 1,
};

static void
eam_dbus_server_finalize (GObject *obj)
{
  EamDbusServerPrivate *priv = eam_dbus_server_get_instance_private (EAM_DBUS_SERVER (obj));

  g_main_loop_unref (priv->mainloop);

  if (priv->busowner > 0)
    g_bus_unown_name (priv->busowner);

  if (priv->hangup > 0)
    g_source_remove (priv->hangup);

  if (priv->terminate > 0)
    g_source_remove (priv->terminate);

  if (priv->interrupt > 0)
    g_source_remove (priv->interrupt);

  if (priv->quit_id > 0)
    g_signal_handler_disconnect (priv->service, priv->quit_id);

  if (priv->timer_id > 0)
    g_source_remove (priv->timer_id);

  g_clear_object (&priv->service);

  G_OBJECT_CLASS (eam_dbus_server_parent_class)->finalize (obj);
}

static void
quit_request_cb (EamDbusServer *server, gpointer data)
{
  eam_dbus_server_quit (server);
}

static gboolean
timeout_cb (gpointer data)
{
  EamDbusServerPrivate *priv = eam_dbus_server_get_instance_private (EAM_DBUS_SERVER (data));
  guint idle = eam_service_get_idle (priv->service);

  if (idle > eam_config_timeout ()) {
    eam_dbus_server_quit (EAM_DBUS_SERVER (data));
    return FALSE;
  }

  return TRUE;
}

static void
eam_dbus_server_set_property (GObject *obj, guint prop_id, const GValue *value,
  GParamSpec *pspec)
{
  EamDbusServerPrivate *priv = eam_dbus_server_get_instance_private (EAM_DBUS_SERVER (obj));

  switch (prop_id) {
  case PROP_DB:
    priv->service = eam_service_new (g_value_get_object (value));
    priv->quit_id = g_signal_connect_swapped (priv->service, "quit-requested",
      G_CALLBACK (quit_request_cb), obj);
    if (eam_config_timeout () > 0) {
      /* check every minute if the service is idle, but terminate it only
       * if the configured time has elapsed
       */
      priv->timer_id = g_timeout_add_seconds (60, timeout_cb, obj);
      g_source_set_name_by_id (priv->timer_id, "[EAM] timeout poll");
    }
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
eam_dbus_server_class_init (EamDbusServerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = eam_dbus_server_finalize;
  object_class->set_property = eam_dbus_server_set_property;

  /**
   * EamDbusServer:db:
   *
   * The #EamPkdb to handle by this server
   */
  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "database", "", EAM_TYPE_PKGDB,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

#ifdef G_OS_UNIX
static gboolean
signal_hangup (gpointer data)
{
  EamDbusServer *server = EAM_DBUS_SERVER (data);
  EamDbusServerPrivate *priv = eam_dbus_server_get_instance_private (server);

  /* If we're reloading, voluntarily relinquish our bus
   * name to avoid triggering a "bus-name-lost" signal.
   */
  g_bus_unown_name (priv->busowner);
  priv->busowner = 0;

  eam_log_debug_message ("Received SIGHUP - terminating.");
  eam_dbus_server_quit (EAM_DBUS_SERVER (data));

  return FALSE;
}

static gboolean
signal_terminate (gpointer data)
{
  eam_log_debug_message ("Received SIGTERM - terminating.");
  eam_dbus_server_quit (EAM_DBUS_SERVER (data));

  return FALSE;
}

static gboolean
signal_interrupt (gpointer data)
{
  eam_log_debug_message ("Received SIGINT - terminating.");

  eam_dbus_server_quit (EAM_DBUS_SERVER (data));

  return FALSE;
}
#endif

static void
eam_dbus_server_init (EamDbusServer *server)
{
  EamDbusServerPrivate *priv = eam_dbus_server_get_instance_private (server);

  priv->mainloop = g_main_loop_new (NULL, FALSE);

#ifdef G_OS_UNIX
  priv->hangup = g_unix_signal_add (SIGHUP, signal_hangup, server);
  priv->terminate = g_unix_signal_add (SIGTERM, signal_terminate, server);
  priv->interrupt = g_unix_signal_add (SIGINT, signal_interrupt, server);
#endif
}

#ifdef EAM_ENABLE_DEBUG
static void
print_peer_credentials (GDBusConnection *connection)
{
  GCredentials *credentials = g_dbus_connection_get_peer_credentials (connection);
  if (credentials != NULL)
    {
      char *s = g_credentials_to_string (credentials);

      eam_log_debug_message ("Peer credentials: %s.", s != NULL ? s : "none");

      g_free (s);
    }

  eam_log_debug_message ("Negotiated capabilities: unix-fd-passing = %s",
    (g_dbus_connection_get_capabilities (connection) & G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING) != 0
      ? "yes"
      : "no");
}
#endif

static void
on_bus_acquired (GDBusConnection *connection, const gchar *name, gpointer data)
{
  EamDbusServer *server = EAM_DBUS_SERVER (data);
  EamDbusServerPrivate *priv = eam_dbus_server_get_instance_private (server);

#ifdef EAM_ENABLE_DEBUG
  eam_log_debug_message ("bus acquired: %s", name);
  print_peer_credentials (connection);
#endif

  eam_service_dbus_register (priv->service, connection);
}

static void
on_name_acquired (GDBusConnection *connection, const gchar *name, gpointer data)
{
  eam_log_debug_message ("name acquired: %s", name);
}

static void
on_name_lost (GDBusConnection *connection, const gchar *name, gpointer data)
{
  eam_log_debug_message ("name lost: %s", name);
  eam_dbus_server_quit (EAM_DBUS_SERVER (data));
}

/**
 * eam_dbus_server_new:
 * @db: A #EamPkgdb instance.
 *
 * Returns: a #EamDbusServer.
 **/
EamDbusServer *
eam_dbus_server_new (EamPkgdb *db)
{
  return g_object_new (EAM_TYPE_DBUS_SERVER, "db", db, NULL);
}

/**
 * eam_dbus_server_run:
 * @server: a #EamDbusServer.
 *
 * Acquire the DBus name "com.endlesssm.AppManager" and launch de
 * AppManager service.
 *
 * Returns: %TRUE if the server is launched. Otherwise %FALSE;
 **/
gboolean
eam_dbus_server_run (EamDbusServer *server)
{
  g_return_val_if_fail (EAM_IS_DBUS_SERVER (server), FALSE);

  EamDbusServerPrivate *priv = eam_dbus_server_get_instance_private (server);
  GError *error = NULL;

  if (g_main_loop_is_running (priv->mainloop))
    return FALSE;

  if (!eam_service_load_authority (priv->service, &error)) {
    eam_log_error_message ("Unable to load authority: %s", error->message);
    g_clear_error (&error);
    return FALSE;
  }

  priv->busowner = g_bus_own_name (G_BUS_TYPE_SYSTEM, "com.endlessm.AppManager",
    G_BUS_NAME_OWNER_FLAGS_REPLACE | G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
    on_bus_acquired, on_name_acquired, on_name_lost,
    g_object_ref (server), (GDestroyNotify) g_object_unref);

  g_main_loop_run (priv->mainloop);

  return TRUE;
}

/**
 * eam_dbus_server_quit:
 * @server: a #EamDbusServer.
 *
 * Quits the AppManager DBus service.
 **/
void
eam_dbus_server_quit (EamDbusServer *server)
{
  g_return_if_fail (EAM_IS_DBUS_SERVER (server));

  EamDbusServerPrivate *priv = eam_dbus_server_get_instance_private (server);
  g_main_loop_quit (priv->mainloop);
}
