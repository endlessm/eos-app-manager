#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script copies the previous version installation directory
# to a temporary file.
#
# Usage:
#
# $ ./20-backup.sh <app_id> ...
#
# Parameters:
# <app_id>: ID of the application to updates.
#
# IMPORTANT: This script makes some assumptions that could be subject of
# modification:
# - The application installation directory id ${EAM_PREFIX}/<app_id>.

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"
check_args_minimum_number "${#}" 1 "<app_id>"

APP_ID=$1

cp --archive "${EAM_PREFIX}/${APP_ID}" "${EAM_TMP}/${APP_ID}.old"
