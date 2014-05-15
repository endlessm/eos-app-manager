#!/bin/bash

# Copyright 2014 Endless Mobile, Inc.
#
# This script deletes the bundle and other files downloaded
# from the server during an application update proccess.
#
# Usage: ./05-cleanup-downloads.sh <app_id> <bundle_path> ...
#
# Parameters:
# <app_id>: ID of the application to update.
# <bundle_path>: Path to the downloaded application bundle.
#
# IMPORTANT: This script makes some assumptions that could be subject of
# modification:
# - The SHA256 file is in the same directory than the downloaded bundle and
#   its name is <app_id>.sha256

# This script uses the configuration variables defined in
# ../../install-config.sh:
#  * PREFIX
#  * TMP

SCRIPT_DIR=${BASH_SOURCE[0]%/*}

# Include configuration variables
. ${SCRIPT_DIR}/../../install-config.sh

# Include utilities
. ${SCRIPT_DIR}/../../utils.sh

debug "Running '${BASH_SOURCE[0]}'"

RM=$(which rm) || exit_error "Can't find rm"

ARGS=2
if [ $# -lt "$ARGS" ]
then
  exit_error "Usage: `basename $0` <app_id> <bundle_path>"
fi

APP_ID=$1

# Delete the temporary directory (if exists)
if [ -d "${TMP}/${APP_ID}" ]; then
  $RM --recursive --force "${TMP}/${APP_ID}"
  if [ "$?" -ne 0 ]; then
    warning "To delete '${TMP}/${APP_ID}' failed"
  fi
fi

# Delete dowloaded files (the bundle and the SHA256 file)
BUNDLE_FILE=$2
BUNDLE_DIR=${BUNDLE_FILE%/*}
SHA256_FILE="${BUNDLE_DIR}/${APP_ID}.sha256"

$RM --force $SHA256_FILE
if [ "$?" -ne 0 ]; then
  warning "Failed to remove the sha256file '${SHA256_FILE}'"
fi

$RM --force $BUNDLE_FILE
if [ "$?" -ne 0 ]; then
  warning "Failed to remove the bundle file '${BUNDLE_FILE}'"
fi

exit 0
