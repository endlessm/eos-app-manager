#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script moves the uncompressed bundle to ${EAM_PREFIX}
#
# Usage:
#
# $ ./04-move-update-to-prefix.sh <app_id> ...
#
# Parameters:
# <app_id>: ID of the application.
#
# IMPORTANT: This script makes some assumptions that could be subject of
# modification:
# - The application installation directory will be ${EAM_PREFIX}/<app_id>
#
# Returns 0 on success.

SCRIPT_DIR=${BASH_SOURCE[0]%/*}
. ${SCRIPT_DIR}/../../utils.sh

debug "Running '${BASH_SOURCE[0]}'"

MV=$(which mv) || exit_error "Can't find mv"

ARGS=1
if [ $# -lt "$ARGS" ]
then
  exit_error "Usage: `basename $0` <app_id> .."
fi

APP_ID=$1

# Move the uncompressed bundle to the installation directory
if [ ! -d "${EAM_TMP}/${APP_ID}" ]; then
  exit_error "The extracted bundle is not a directory called '${APP_ID}'"
fi

if [ -d "${EAM_PREFIX}/${APP_ID}" ]; then
  # This should never happen.
  # If it does happen, there is an error in the installation/update process.
  exit_error "A directory called '${EAM_PREFIX}/${APP_ID}' already exists"
fi

${MV} --force "${EAM_TMP}/${APP_ID}" "${EAM_PREFIX}"
if [ "$?" -ne 0 ]; then
  exit_error "To move the application from '${EAM_TMP}' to '${EAM_PREFIX}' failed"
fi
