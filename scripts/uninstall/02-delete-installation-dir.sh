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
check_args_minimum_number "${#}" 1 "<app_id>"
APP_ID=$1

rm --recursive "${EAM_PREFIX}/${APP_ID}"
