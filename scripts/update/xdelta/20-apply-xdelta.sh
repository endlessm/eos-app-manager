#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# Applies the downloaded xdelta file to the application installation
# directory.
#
# Usage:
#
# $ ./20-apply-xdelta.sh <app_id> <bundle_file_path>...
#
# Parameters:
# <app_id>: ID of the application
# <bundle_file_path>: Path to the bundle containing the xdelta diffs
#
# IMPORTANT: This script makes some assumptions that could be subject of
# modification:
# - The application installation directory id ${EAM_PREFIX}/<app_id>.

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"
check_args_minimum_number "${#}" 2 "<app_id> <bundle_path>"
APP_ID=$1
BUNDLE=$2

delete_dir "${EAM_TMP}/${APP_ID}" && mkdir "${EAM_TMP}/${APP_ID}"
xdelta3-dir-patcher apply --ignore-euid "${EAM_PREFIX}/${APP_ID}" "${BUNDLE}" "${EAM_TMP}/${APP_ID}"
