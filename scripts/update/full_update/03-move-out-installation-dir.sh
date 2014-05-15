#!/bin/bash

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
# - The application installation directory id ${PREFIX}/<app_id>
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

# Move the old version installation dir
${MV} --force "${PREFIX}/${APP_ID}" "${TMP}/${APP_ID}.old"
if [ "$?" -ne 0 ]; then
  exit_error "To move the old application directory '${PREFIX}/${APP_ID}' to '${TMP}/${APP_ID}.old' failed"
fi

exit 0

