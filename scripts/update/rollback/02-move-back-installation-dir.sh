#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script moves the currently installed version directory
# to a temporary directory.
#
# Usage: ./02-move-back-installation-dir.sh <app_id> ...
#
# Parameters:
# <app_id>: ID of the application to update.
#
# IMPORTANT: This script makes some assumptions that could be subject of
# modification:
# - The application installation directory is ${EAM_PREFIX}/<app_id>

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"

MV=$(which mv)   || exit_error "Can't find mv"

check_args_minimum_number "${#}" 1 "<app_id>"
APP_ID=$1

if [ -d "${EAM_TMP}/${APP_ID}.old" ]; then
  ${MV} --force "${EAM_TMP}/${APP_ID}.old" "${EAM_PREFIX}/${APP_ID}"
  if [ "$?" -ne 0 ]; then
    exit_error "To move back the application from '${EAM_TMP}' to '${EAM_PREFIX}' failed"
  fi
fi

exit 0