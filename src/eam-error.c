#include "config.h"

#include "eam-error.h"

#include <gio/gio.h>

static const GDBusErrorEntry eam_error_entries[] = {
  { EAM_ERROR_FAILED,         "com.endlessm.AppManager.Error.Failed" },
  { EAM_ERROR_PROTOCOL_ERROR, "com.endlessm.AppManager.Error.ProtocolError" },
  { EAM_ERROR_INVALID_FILE,   "com.endlessm.AppManager.Error.InvalidFile" },
  { EAM_ERROR_NO_NETWORK,     "com.endlessm.AppManager.Error.NoNetwork" },
  { EAM_ERROR_PKG_UNKNOWN,    "com.endlessm.AppManager.Error.UnknownPackage" },
  { EAM_ERROR_UNIMPLEMENTED,  "com.endlessm.AppManager.Error.Unimplemented" },
  { EAM_ERROR_AUTHORIZATION,  "com.endlessm.AppManager.Error.Authorization" },
  { EAM_ERROR_NOT_AUTHORIZED, "com.endlessm.AppManager.Error.NotAuthorized" },
};

GQuark
eam_error_quark (void)
{
  static volatile gsize quark_volatile = 0;

  g_dbus_error_register_error_domain ("eam-error-quark",
                                      &quark_volatile,
                                      eam_error_entries,
                                      G_N_ELEMENTS (eam_error_entries));

  return quark_volatile;
}
