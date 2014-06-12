#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script cleans temporary files created
# during an application update, including the
# moved old version installation directory.
#
# Usage: ./07-cleanup.sh <app_id> <bundle_path> ...
#
# Parameters:
# <app_id>: ID of the application.
# <bundle_path>: Path to the downloaded application bundle.
#
# IMPORTANT: This script makes some assumptions that could be subject of
# modification:
# - The name of the SHA256 file and GPG signature are <app_id>.sha256 and
# <app_id>.asc, respectively.
# - The old version installation directory was moved and renamed as
#   '${EAM_TMP}/${APP_ID}.old'

SCRIPT_DIR=${BASH_SOURCE[0]%/*}
. ${SCRIPT_DIR}/../../config.sh
. ${SCRIPT_DIR}/../../utils.sh

debug "Running '${BASH_SOURCE[0]}'"

RM=$(which rm) || exit_error "Can't find rm"

ARGS=2
if [ $# -lt "$ARGS" ]
then
  exit_error "Usage: `basename $0` <app_id> <bundle_path>"
fi

APP_ID=$1
BUNDLE_FILE=$2

delete_download "${BUNDLE_FILE}" "${APP_ID}.sha256" "${APP_ID}.asc"

# Delete the old version installation directory
$RM --recursive --force "${EAM_TMP}/${APP_ID}.old"
if [ "$?" -ne 0 ]; then
  warning "To delete '${EAM_TMP}/${APP_ID}' failed"
fi

exit 0
