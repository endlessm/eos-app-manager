/* eam-error.h: Error codes
 *
 * This file is part of eos-app-manager.
 * Copyright 2014  Endless Mobile Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
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
