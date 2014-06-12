#!/bin/bash -e

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
# - The application intallation directory is named ${EAM_PREFIX}/<app_id>.

. ${BASH_SOURCE[0]%/*}/../utils.sh

print_header "${BASH_SOURCE[0]}"

RM=$(which rm) || exit_error "Can't find rm"

check_args_minimum_number "${#}" 1 "<app_id>"
APP_ID=$1

# Delete the installation directory
$RM --recursive --force "${EAM_PREFIX}/${APP_ID}"
if [ "$?" -ne 0 ]; then
  exit_error "To delete the installation directory '${EAM_PREFIX}/${APP_ID}' failed"
fi

exit 0
