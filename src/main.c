// Copyright © 2014  Endless Mobile

#include "config.h"

#include <stdlib.h>
#include <glib/gi18n.h>

#include "eam-config.h"
#include "eam-pkgdb.h"
#include "eam-dbus-server.h"
#include "eam-fs-sanity.h"

static const gchar *opt_cfgfile;
static gboolean opt_dumpcfg;
static gboolean opt_dump_pkgdb;

static gboolean
parse_options (int *argc, gchar ***argv)
{
  GError *err = NULL;
  GOptionEntry entries[] = {
    { "config", 'c', 0, G_OPTION_ARG_FILENAME, &opt_cfgfile, N_ ("Configuration file"), NULL },
    { "dump", 'd', 0, G_OPTION_ARG_NONE, &opt_dumpcfg, N_ ("Dump configuration"), NULL },
    { "dump-pkgdb", 'l', 0, G_OPTION_ARG_NONE, &opt_dump_pkgdb, N_ ("Dump package database"), NULL },
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

static gboolean
load_config ()
{
  GKeyFile *keyfile = NULL;
  gboolean ret = TRUE;

  if (!opt_cfgfile)
    goto bail;

  GError *err = NULL;
  keyfile = g_key_file_new ();
  if (!g_key_file_load_from_file (keyfile, opt_cfgfile, G_KEY_FILE_NONE, &err)) {
    g_warning (N_ ("Error parsing configuration file: %s"), err->message);
    g_key_file_unref (keyfile);
    g_error_free (err);
    keyfile = NULL;
  }

bail:
  if (!eam_config_load (eam_config_get (), keyfile)) {
    g_warning (N_ ("Cannot load configuration parameters"));
    ret = FALSE;
  }

  if (keyfile)
    g_key_file_unref (keyfile);

  return ret;
}

static gboolean
dump_pkgdb (EamPkgdb * db)
{
  GError *error = NULL;

  g_return_val_if_fail (EAM_IS_PKGDB (db), FALSE);

  eam_pkgdb_load (db, &error);
  if (error) {
    g_warning (N_ ("Cannot load the package database"));
    g_clear_error(&error);

    return FALSE;
  }

  eam_pkgdb_dump (db);
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

  if (!load_config ())
    return EXIT_FAILURE;

  if (opt_dumpcfg) {
    eam_config_dump (NULL);
    return EXIT_SUCCESS;
  }

  if (!eam_fs_sanity_check ())
    return EXIT_FAILURE;

  EamPkgdb *db = eam_pkgdb_new_with_appdir (eam_config_appdir ());

  if (opt_dump_pkgdb) {
    if (dump_pkgdb (db))
      ret = EXIT_SUCCESS;

    g_object_unref (db);
    return ret;
  }

  EamDbusServer *server = eam_dbus_server_new (db);

  if (eam_dbus_server_run (server))
    ret = EXIT_SUCCESS;

  g_object_unref (server);
  eam_config_destroy (NULL);

  return ret;
}
