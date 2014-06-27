#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script verifies the downloaded EndlessOS Bundle and checks its
# integrity, extracts the bundle into a temporary file. The bundle will
# contain a xdelta diff file.
#
# Usage:
#
# $ ./10-verify-downloads.sh <app_id> <bundle_file_path> ...
#
# Parameters:
# <app_id>: ID of the application.
# <bundle_path>: Path to the downloaded application xdelta diff
#
# IMPORTANT: This script makes some assumptions that could be subject of
# modification:
# - The SHA256 file and the GPG signature are called <app_id>.sha256 and
#   <app_id>.asc, respectively.
#
# Returns 0 on success.

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"
check_args_minimum_number "${#}" 2 "<app_id> <bundle_path>"
APP_ID=$1
BUNDLE=$2

verify_downloaded_file "${BUNDLE}" "${APP_ID}.sha256" "${APP_ID}.asc"
