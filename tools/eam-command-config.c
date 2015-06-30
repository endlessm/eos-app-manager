#include "config.h"

#include "eam-commands.h"
#include "eam-config.h"

#include <stdlib.h>
#include <string.h>
#include <glib.h>

static void
print_all (void)
{
  g_print ("eam─┬─appdir───%s\n"
           "    ├─downloaddir───%s\n"
           "    ├─serveraddress───%s\n"
           "    ├─protocolversion───%s\n"
           "    ├─scriptdir───%s\n"
           "    ├─timeout───%u\n"
           "    ├─deltaupdates───%s\n"
           "    └─gpgkeyring───%s\n",
           eam_config_appdir (),
           eam_config_dldir (),
           eam_config_saddr (),
           eam_config_protver (),
           eam_config_scriptdir (),
           eam_config_timeout (),
           eam_config_deltaupdates () ? "true" : "false",
           eam_config_gpgkeyring ());
}

int
eam_command_config (int argc, char *argv[])
{
  if (argc == 1) {
    print_all ();
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "list") == 0) {
    g_print ("appdir\n"
             "downloaddir\n"
             "serveraddress\n"
             "protocolversion\n"
             "scriptdir\n"
             "gpgpkeyring\n"
             "timeout\n"
             "deltaupdates\n");
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "appdir") == 0) {
    g_print ("%s\n", eam_config_appdir ());
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "downloaddir") == 0) {
    g_print ("%s\n", eam_config_dldir ());
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "serveraddress") == 0) {
    g_print ("%s\n", eam_config_saddr ());
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "protocolversion") == 0) {
    g_print ("%s\n", eam_config_protver ());
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "scriptdir") == 0) {
    g_print ("%s\n", eam_config_scriptdir ());
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "gpgkeyring") == 0) {
    g_print ("%s\n", eam_config_gpgkeyring ());
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "timeout") == 0) {
    g_print ("%u\n", eam_config_timeout ());
    return EXIT_SUCCESS;
  }

  if (strcmp (argv[1], "deltaupdates") == 0) {
    g_print ("%s\n", eam_config_deltaupdates () ? "true" : "false");
    return EXIT_SUCCESS;
  }

  g_printerr ("Unknown configuration key '%s'\n", argv[1]);

  return EXIT_FAILURE;
}
