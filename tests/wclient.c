/* Copyright 2014 Endless Mobile, Inc. */

#include <stdlib.h>
#include <glib.h>
#include "eam-wc.h"

static char **arguments;
static char *outfile;

static void
wc_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  GError *error = NULL;
  gboolean res = eam_wc_request_finish (EAM_WC (source), result, NULL, NULL, &error);
  g_print ("%s\n", res ? "Success!" : "Failure");
  if (error) {
    g_printerr ("Error: %s", error->message);
    g_error_free (error);
  }

  g_main_loop_quit (data);
}

static gboolean
parse_options (int *argc, gchar ***argv)
{
  GError *err = NULL;
  GOptionEntry entries[] = {
    { "output", 'o', 0, G_OPTION_ARG_FILENAME, &outfile, "Output file path", NULL },
    { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &arguments, "", "URL …"},
    { NULL },
  };

 GOptionContext *ctxt = g_option_context_new ("- EndlessOS Web Client");
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
main (gint argc, char **argv)
{
  if (!parse_options (&argc, &argv))
    return EXIT_FAILURE;

  if (!arguments) {
    g_print ("You need to specify a URL to download\n");
    return EXIT_FAILURE;
  }

  EamWc *wc = eam_wc_new ();
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);

  g_object_set (wc, "filename", outfile, NULL);
  eam_wc_request_async (wc, arguments[0], NULL, wc_cb, loop);

  g_main_loop_run (loop);

  g_strfreev (arguments);
  g_free (outfile);
  g_object_unref (wc);
  g_main_loop_unref (loop);

  return 0;
}
