#!/bin/bash

# Copyright 2014 Endless Mobile, Inc.
#
# This script cleans temporary files created
# during an application installation.
#
# Usage: ./04-clean.sh <app_id> <bundle_path> ...
#
# Parameters:
# <app_id>: ID of the application to install.
# <bundle_path>: Path to the downloaded application bundle.
#
# IMPORTANT: This script makes some assumptions that could be subject of
# modification:
# - The SHA256 file is in the same directory than the downloaded bundle and
#   its name is <app_id>.sha256

# Include utilities
SCRIPT_DIR=${BASH_SOURCE[0]%/*}
. ${SCRIPT_DIR}/../utils.sh

RM=$(which rm) || exit_error "Can't find rm"

ARGS=2
if [ $# -lt "$ARGS" ]
then
  exit_error "Usage: `basename $0` <app_id> <bundle_path>"
fi

APP_ID=$1
BUNDLE_FILE=$2
BUNDLE_DIR=${BUNDLE_FILE%/*}
SHA256_FILE="${BUNDLE_DIR}/${APP_ID}.sha256"

$RM $SHA256_FILE
if [ "$?" -ne 0 ]; then
  warning "Failed to remove the sha256file '${SHA256_FILE}'"
fi

$RM $BUNDLE_FILE
if [ "$?" -ne 0 ]; then
  warning "Failed to remove the bundle file '${BUNDLE_FILE}'"
fi

exit 0
