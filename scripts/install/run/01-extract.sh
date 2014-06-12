#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script verifies the downloaded EndlessOS Bundle and checks its
# integrity, extracts the bundle into a temporary file and finally moves
# it to its corresponding installation directory.
#
# Usage:
#
# $ ./01-extract.sh <app_id> <bundle_file_path> ...
#
# Parameters:
# <app_id>: ID of the application to install.
# <bundle_path>: Path to the downloaded application bundle (a .tar.gz file)
#
# IMPORTANT: This script makes some assumptions that could be subject of
# modification:
# - The SHA256 file and the GPG signature are called <app_id>.sha256 and
#   <app_id>.asc, respectively.
# - The bundle file is formed by a directory, called <app_id>, that contains
#   the application data.
# - The application installation directory will be ${EAM_PREFIX}/<app_id>
#
# Returns 0 on success.

SCRIPT_DIR=${BASH_SOURCE[0]%/*}
. ${SCRIPT_DIR}/../../utils.sh

debug "Running '${BASH_SOURCE[0]}'"

MV=$(which mv)   || exit_error "Can't find mv"

ARGS=2
if [ $# -lt "$ARGS" ]
then
  exit_error "Usage: `basename $0` <app_id> <bundle_file_path> ..."
fi

APP_ID=$1
BUNDLE=$2

verify_download "${BUNDLE}" "${APP_ID}.sha256" "${APP_ID}.asc"
extract_file_to "${BUNDLE}" "${EAM_TMP}"

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
