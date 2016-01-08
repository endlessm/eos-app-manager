/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_ERROR_H
#define EAM_ERROR_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * EamError:
 * @EAM_ERROR_FAILED: Operation failed.
 * @EAM_ERROR_PROTOCOL_ERROR: Invalid URI.
 * @EAM_ERROR_INVALID_FILE: Invalid JSON file.
 * @EAM_ERROR_NO_NETWORK: The network is not available.
 * @EAM_ERROR_PKG_UNKNOWN: Requested package is unknown.
 * @EAM_ERROR_UNIMPLEMENTED: Asked for an unimplemented feature.
 * @EAM_ERROR_AUTHORIZATION: Authorization not granted.
 * @EAM_ERROR_NOT_AUTHORIZED: Operation not authorized.
 * @EAM_ERROR_NOT_ENOUGH_DISK_SPACE: Not enough disk space.
 *
 * These constants identify all the available errors managed by
 * the Endless Application Manager.
 */
typedef enum {
  EAM_ERROR_FAILED = 1,
  EAM_ERROR_PROTOCOL_ERROR,
  EAM_ERROR_INVALID_FILE,
  EAM_ERROR_NO_NETWORK,
  EAM_ERROR_PKG_UNKNOWN,
  EAM_ERROR_UNIMPLEMENTED,
  EAM_ERROR_AUTHORIZATION,
  EAM_ERROR_NOT_AUTHORIZED,
  EAM_ERROR_NOT_ENOUGH_DISK_SPACE
} EamError;

#define EAM_ERROR eam_error_quark ()

GQuark eam_error_quark (void) G_GNUC_CONST;

G_END_DECLS

#endif /* EAM_ERROR_H */
