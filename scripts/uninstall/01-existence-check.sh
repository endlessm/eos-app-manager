#!/bin/bash

# Copyright 2014 Endless Mobile, Inc.
#
# This script checks if the application installation directory
# exists.
#
# Usage: ./01-existence-check.sh <app_id> ...
#
# Parameters:
# <app_id>: ID of the application to uninstall.
#
# IMPORTANT: This script makes some assumptions that could be subject of
# modification:
# - The application installation directory is named ${EAM_PREFIX}/<app-id>.

SCRIPT_DIR=${BASH_SOURCE[0]%/*}
. ${SCRIPT_DIR}/../config.sh
. ${SCRIPT_DIR}/../utils.sh

debug "Running '${BASH_SOURCE[0]}'"

ARGS=1
if [ $# -lt "$ARGS" ]
then
  exit_error "Usage: `basename $0` <app_id> .."
fi

APP_ID=$1

# Check if the application is installed
if [ ! -d "${EAM_PREFIX}/${APP_ID}" ]; then
  exit_error "Application '${APP_ID}' not found. The installation directory \
'${EAM_PREFIX}/${APP_ID}' does not exist."
fi

exit 0
