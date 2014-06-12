#!/bin/bash

# Copyright 2014 Endless Mobile, Inc.
#
# This script deletes the application installation directory.
#
# Usage: ./03-delete-installation-dir.sh <app_id> ...
#
# Parameters:
# <app_id>: ID of the application to install.
#
# IMPORTANT: This script makes some assumptions that could be subject of
# modification:
# - The application intallation directory is named ${PREFIX}/<app_id>.

SCRIPT_DIR=${BASH_SOURCE[0]%/*}
. ${SCRIPT_DIR}/../config.sh
. ${SCRIPT_DIR}/../utils.sh

debug "Running '${BASH_SOURCE[0]}'"

RM=$(which rm) || exit_error "Can't find rm"

ARGS=1
if [ $# -lt "$ARGS" ]; then
  exit_error "Usage: `basename $0` <app_id>"
fi

APP_ID=$1

# Delete the installation directory
$RM --recursive --force "${PREFIX}/${APP_ID}"
if [ "$?" -ne 0 ]; then
  exit_error "To delete the installation directory '${PREFIX}/${APP_ID}' failed"
fi

exit 0
