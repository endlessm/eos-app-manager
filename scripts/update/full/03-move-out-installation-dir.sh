#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script moves the previous version installation directory
# to a temporary file.
#
# Usage:
#
# $ ./03-move-out-installation-dir.sh <app_id> ...
#
# Parameters:
# <app_id>: ID of the application.
#
# IMPORTANT: This script makes some assumptions that could be subject of
# modification:
# - The application installation directory id ${EAM_PREFIX}/<app_id>
#
# Returns 0 on success.

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"

MV=$(which mv) || exit_error "Can't find mv"

check_args_minimum_number "${#}" 1 "<app_id>"
APP_ID=$1

# Move the old version installation dir
${MV} --force "${EAM_PREFIX}/${APP_ID}" "${EAM_TMP}/${APP_ID}.old"
if [ "$?" -ne 0 ]; then
  exit_error "To move the old application directory '${EAM_PREFIX}/${APP_ID}' to '${EAM_TMP}/${APP_ID}.old' failed"
fi

exit 0

