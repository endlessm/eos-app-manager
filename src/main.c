/* Copyright © 2014  Endless Mobile */

#include "config.h"

#include <stdlib.h>
#include <glib/gi18n.h>

#include "eam-dbus-server.h"

int
main (int argc, gchar **argv)
{
  int ret = EXIT_FAILURE;

#ifdef ENABLE_NLS
  /* Initialize the i18n stuff */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  EamDbusServer *server = eam_dbus_server_new ();

  if (eam_dbus_server_run (server))
    ret = EXIT_SUCCESS;

  g_object_unref (server);

  return ret;
}
