#!/bin/bash -e

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
# - The name of the SHA256 file and GPG signature are <app_id>.sha256 and
# <app_id>.asc, respectively.

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"
check_args_minimum_number "${#}" 2 "<app_id> <bundle_path>"
APP_ID=$1
BUNDLE=$2

delete_downloaded_file "${BUNDLE}" "${APP_ID}.sha256" "${APP_ID}.asc"
