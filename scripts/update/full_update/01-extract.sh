#!/bin/bash

# Copyright 2014 Endless Mobile, Inc.
#
# This script checks the SHA256 integrity of a downloaded EndlessOS Bundle,
# and extracts the bundle into a temporary file.
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
# - The SHA256 file is in the same directory than the downloaded bundle and
#   its name is <app_id>.sha256
# - The tar.gz file  is formed by a directory, called <app_id>, that contains
#   the application data.
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

SHA256SUM=$(which sha256sum) || exit_error "Can't find sha256sum"
TAR=$(which tar) || exit_error "Can't find tar"
MV=$(which mv)   || exit_error "Can't find mv"

ARGS=2
if [ $# -lt "$ARGS" ]
then
  exit_error "Usage: `basename $0` <app_id> <bundle_file_path> ..."
fi

APP_ID=$1
BUNDLE=$2

# Check downloaded bundle integrity
if [ ! -e "$BUNDLE" ]; then
  exit_error "Bundle file '${BUNDLE}' does not exists"
fi

BUNDLE_DIR=${BUNDLE%/*}
SHA256="${APP_ID}.sha256"
if [ ! -e "${BUNDLE_DIR}/${SHA256}" ]; then
  exit_error "SHA256 file '${BUNDLE_DIR}/${SHA256}' does not exists"
fi

cd $BUNDLE_DIR && $SHA256SUM --quiet --status --check $SHA256
if [ "$?" -ne 0 ]; then
  exit_error "The downloaded bundle '${BUNDLE}' is corrupted (SHA256 does not match)"
fi

# Untar the bundle to a temporary directory
${TAR} --no-same-owner --extract --file=$BUNDLE  --directory=$TMP
if [ "$?" -ne 0 ]; then
  exit_error "To uncompress the bundle '${BUNDLE}' to directory '${TMP}' failed"
fi
