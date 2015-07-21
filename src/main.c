/* Copyright © 2014  Endless Mobile */

#include "config.h"

#include <stdlib.h>
#include <glib/gi18n.h>

#include "eam-config.h"
#include "eam-dbus-server.h"
#include "eam-log.h"

static const gchar *opt_cfgfile;
static gboolean opt_dumpcfg;

static gboolean
parse_options (int *argc, gchar ***argv)
{
  GError *err = NULL;
  GOptionEntry entries[] = {
    { "config", 'c', 0, G_OPTION_ARG_FILENAME, &opt_cfgfile, N_ ("Configuration file"), NULL },
    { "dump", 'd', 0, G_OPTION_ARG_NONE, &opt_dumpcfg, N_ ("Dump configuration"), NULL },
    { NULL },
  };

 GOptionContext *ctxt = g_option_context_new (N_ ("- EndlessOS Application Manager"));
 g_option_context_add_main_entries (ctxt, entries, NULL);
 if (!g_option_context_parse (ctxt, argc, argv, &err)) {
   g_print ("Error parsing options: %s\n", err->message);
   g_error_free (err);
   return FALSE;
 }

 g_option_context_free (ctxt);

 return TRUE;
}

int
main (int argc, gchar **argv)
{
  gint ret = EXIT_FAILURE;

#ifdef ENABLE_NLS
  /* Initialize the i18n stuff */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  if (!parse_options (&argc, &argv))
    return EXIT_FAILURE;

  EamDbusServer *server = eam_dbus_server_new ();

  if (eam_dbus_server_run (server))
    ret = EXIT_SUCCESS;

  g_object_unref (server);

  return ret;
}
