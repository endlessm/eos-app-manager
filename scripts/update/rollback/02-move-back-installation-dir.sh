#!/bin/bash

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
# - The application installation directory is ${PREFIX}/<app_id>

SCRIPT_DIR=${BASH_SOURCE[0]%/*}
. ${SCRIPT_DIR}/../../install-config.sh
. ${SCRIPT_DIR}/../../utils.sh

debug "Running '${BASH_SOURCE[0]}'"

MV=$(which mv)   || exit_error "Can't find mv"

APP_ID=$1

if [ -d "${TMP}/${APP_ID}.old" ]; then
  ${MV} --force "${TMP}/${APP_ID}.old" "${PREFIX}/${APP_ID}"
  if [ "$?" -ne 0 ]; then
    exit_error "To move back the application from '${TMP}' to '${PREFIX}' failed"
  fi
fi

exit 0