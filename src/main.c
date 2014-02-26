#include <stdlib.h>
#include <glib/gi18n.h>

#include "eam-pkgdb.h"
#include "eam-dbus-server.h"

gchar *opt_appdir;

static gboolean
parse_options (int *argc, gchar ***argv)
{
  GError *err = NULL;
  GOptionEntry entries[] = {
    { "appdir", 'd', 0, G_OPTION_ARG_STRING, &opt_appdir, N_ ("Applications directory"), NULL },
    { NULL },
  };

 GOptionContext *ctxt = g_option_context_new (N_ ("- EndlessOS Application Manager"));
 g_option_context_add_main_entries (ctxt, entries, NULL);
 if (!g_option_context_parse (ctxt, argc, argv, &err)) {
   g_print ("Error parsing options: %s\n", err->message);
   g_error_free (err);
   return FALSE;
 }

 if (opt_appdir == NULL)
   opt_appdir = "/endless";

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

  EamPkgdb *db = eam_pkgdb_new_with_appdir (opt_appdir);
  eam_pkgdb_load (db);

  EamDbusServer *server = eam_dbus_server_new ();
  if (eam_dbus_server_run (server))
	  ret = EXIT_SUCCESS;

  g_object_unref (db);
  g_object_unref (server);

  return ret;
}
