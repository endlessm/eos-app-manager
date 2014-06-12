#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script verifies the downloaded EndlessOS Bundle and checks its
# integrity, extracts the bundle into a temporary file.
#
# Usage:
#
# $ ./01-extract.sh <app_id> <bundle_file_path> ...
#
# Parameters:
# <app_id>: ID of the application.
# <bundle_path>: Path to the downloaded application bundle (a .tar.gz file)
#
# IMPORTANT: This script makes some assumptions that could be subject of
# modification:
# - The downloaded bundle is a tar.gz file.
# - The SHA256 file and the GPG signature are called <app_id>.sha256 and
#   <app_id>.asc, respectively.
# - The tar.gz file  is formed by a directory, called <app_id>, that contains
#   the application data.
# - The application installation directory will be ${EAM_PREFIX}/<app_id>
#
# Returns 0 on success.

SCRIPT_DIR=${BASH_SOURCE[0]%/*}
. ${SCRIPT_DIR}/../../config.sh
. ${SCRIPT_DIR}/../../utils.sh

debug "Running '${BASH_SOURCE[0]}'"

TAR=$(which tar) || exit_error "Can't find tar"
MV=$(which mv)   || exit_error "Can't find mv"

ARGS=2
if [ $# -lt "$ARGS" ]
then
  exit_error "Usage: `basename $0` <app_id> <bundle_file_path> ..."
fi

APP_ID=$1
BUNDLE=$2

verify_download "${BUNDLE}" "${APP_ID}.sha256" "${APP_ID}.asc"

# Untar the bundle to a temporary directory
${TAR} --no-same-owner --extract --file=$BUNDLE  --directory=$EAM_TMP
if [ "$?" -ne 0 ]; then
  exit_error "To uncompress the bundle '${BUNDLE}' to directory '${EAM_TMP}' failed"
fi
