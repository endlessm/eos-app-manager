#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script deletes the bundle and other files downloaded
# from the server during an application installation proccess.
#
# Usage: ./02-cleanup.sh <app_id> <bundle_path> ...
#
# Parameters:
# <app_id>: ID of the application to install.
# <bundle_path>: Path to the downloaded application bundle.
#
# IMPORTANT: This script makes some assumptions that could be subject of
# modification:
# - The SHA256 file is in the same directory than the downloaded bundle and
#   its name is <app_id>.sha256
# - The GPG signature is in the same directory than the downloaded bundle and
#   its name is <app_id>.asc

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

# Delete the installation directory
if [ -d "${EAM_PREFIX}/${APP_ID}" ]; then
  $RM --recursive --force "${EAM_PREFIX}/${APP_ID}"
  if [ "$?" -ne 0 ]; then
    exit_error "To delete the installation directory '${EAM_PREFIX}/${APP_ID}' failed"
  fi
fi

# Delete the temporary directory (if exists)
if [ -d "${EAM_TMP}/${APP_ID}" ]; then
  $RM --recursive --force "${EAM_TMP}/${APP_ID}"
  if [ "$?" -ne 0 ]; then
    warning "To delete '${EAM_TMP}/${APP_ID}' failed"
  fi
fi

# Delete dowloaded files (the bundle and the SHA256 file)
BUNDLE_FILE=$2
BUNDLE_DIR=${BUNDLE_FILE%/*}
SHA256_FILE="${BUNDLE_DIR}/${APP_ID}.sha256"
GPG_FILE="${BUNDLE_DIR}/${APP_ID}.asc"

$RM --force $SHA256_FILE
if [ "$?" -ne 0 ]; then
  warning "Failed to remove the sha256file '${SHA256_FILE}'"
fi

$RM --force "${GPG_FILE}"

$RM --force $BUNDLE_FILE
if [ "$?" -ne 0 ]; then
  warning "Failed to remove the bundle file '${BUNDLE_FILE}'"
fi

exit 0
