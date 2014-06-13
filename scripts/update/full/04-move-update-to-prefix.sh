#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script moves the uncompressed bundle to ${EAM_PREFIX}
#
# Usage:
#
# $ ./04-move-update-to-prefix.sh <app_id> ...
#
# Parameters:
# <app_id>: ID of the application.
#
# IMPORTANT: This script makes some assumptions that could be subject of
# modification:
# - The application installation directory will be ${EAM_PREFIX}/<app_id>
#
# Returns 0 on success.

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"
check_args_minimum_number "${#}" 1 "<app_id>"
APP_ID=$1

mv "${EAM_TMP}/${APP_ID}" "${EAM_PREFIX}"
