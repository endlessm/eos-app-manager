#!/bin/bash

# Copyright 2014 Endless Mobile, Inc.
#
# This script moves the uncompressed bundle to ${PREFIX}
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
# - The application installation directory will be ${PREFIX}/<app_id>
#
# Returns 0 on success.

# This script uses the configuration variables defined in
# ../../install-config.sh:
#  * PREFIX
#  * TMP

SCRIPT_DIR=${BASH_SOURCE[0]%/*}

# Include configuration variables
. ${SCRIPT_DIR}/../../install-config.sh

# # Include utilities
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
if [ ! -d "${TMP}/${APP_ID}" ]; then
  exit_error "The extracted bundle is not a directory called '${APP_ID}'"
fi

if [ -d "${PREFIX}/${APP_ID}" ]; then
  # This should never happen.
  # If it does happen, there is an error in the installation/update process.
  exit_error "A directory called '${PREFIX}/${APP_ID}' already exists"
fi

${MV} --force "${TMP}/${APP_ID}" "${PREFIX}"
if [ "$?" -ne 0 ]; then
  exit_error "To move the application from '${TMP}' to '${PREFIX}' failed"
fi