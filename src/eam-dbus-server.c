/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"

#include <gio/gio.h>
#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif

#include "eam-dbus-server.h"

typedef struct _EamDbusServerPrivate EamDbusServerPrivate;

struct _EamDbusServerPrivate {
  GMainLoop *mainloop;
  guint busowner;
  guint hangup;
  guint terminate;
  guint interrupt;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamDbusServer, eam_dbus_server, G_TYPE_OBJECT)

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

  G_OBJECT_CLASS (eam_dbus_server_parent_class)->finalize (obj);
}

static void
eam_dbus_server_class_init (EamDbusServerClass *class)
{
  G_OBJECT_CLASS (class)->finalize = eam_dbus_server_finalize;
}

#ifdef G_OS_UNIX
static gboolean
signal_hangup (gpointer data)
{
  EamDbusServer *server = EAM_DBUS_SERVER (data);
  EamDbusServerPrivate *priv = eam_dbus_server_get_instance_private (server);

  /* If we're reloading, voluntarily relinquish our bus
   * name to avoid triggering a "bus-name-lost" signal. */
  g_bus_unown_name (priv->busowner);
  priv->busowner = 0;

  eam_dbus_server_quit (EAM_DBUS_SERVER (data));
  return FALSE;
}

static gboolean
signal_terminate (gpointer data)
{
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
  priv->interrupt = g_unix_signal_add (SIGINT, signal_terminate, server);
#endif
}

static void
print_peer_credentials (GDBusConnection *connection)
{
  GCredentials *credentials;
  gchar *s = NULL;

  credentials = g_dbus_connection_get_peer_credentials(connection);
  if (credentials)
    s = g_credentials_to_string (credentials);

  if (s)
    g_debug ("Peer credentials: %s\n", s);
  g_debug ("Negotiated capabilities: unix-fd-passing = %d",
    g_dbus_connection_get_capabilities (connection) & G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING);

  g_free (s);
}

static void
on_bus_acquired (GDBusConnection *connection, const gchar *name, gpointer data)
{
  g_debug ("bus acquired: %s", name);
  print_peer_credentials (connection);
}

static void
on_name_acquired (GDBusConnection *connection, const gchar *name, gpointer data)
{
  g_debug ("name acquired: %s", name);
}

static void
on_name_lost (GDBusConnection *connection, const gchar *name, gpointer data)
{
  g_debug ("name lost: %s", name);
  eam_dbus_server_quit (EAM_DBUS_SERVER (data));
}

EamDbusServer *
eam_dbus_server_new ()
{
  return g_object_new (EAM_TYPE_DBUS_SERVER, NULL);
}

gboolean
eam_dbus_server_run (EamDbusServer *server)
{
  g_return_val_if_fail (EAM_IS_DBUS_SERVER (server), FALSE);

  EamDbusServerPrivate *priv = eam_dbus_server_get_instance_private (server);

  if (g_main_loop_is_running (priv->mainloop))
    return FALSE;

  priv->busowner = g_bus_own_name (G_BUS_TYPE_SESSION, "com.Endless.AppManager",
    G_BUS_NAME_OWNER_FLAGS_REPLACE | G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
    on_bus_acquired, on_name_acquired, on_name_lost,
    g_object_ref (server), (GDestroyNotify) g_object_unref);

  g_main_loop_run (priv->mainloop);

  return TRUE;
}

void
eam_dbus_server_quit (EamDbusServer *server)
{
  g_return_if_fail (EAM_IS_DBUS_SERVER (server));

  EamDbusServerPrivate *priv = eam_dbus_server_get_instance_private (server);
  g_main_loop_quit (priv->mainloop);
}
