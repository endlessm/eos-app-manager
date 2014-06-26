#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script cleans external downloaded files.
#
# Usage: ./40-cleanup-external.sh <app_id>
#
# Parameters:
# <app_id>: ID of the application to install.
#

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"
check_args_minimum_number "${#}" 1 "<app_id>"
APP_ID=$1

external_dir="${EAM_TMP}/${APP_ID}/external"

delete_dir "${external_dir}"
