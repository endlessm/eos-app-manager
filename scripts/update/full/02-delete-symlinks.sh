#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script deletes symbolic links that point
# to the application's metadata files.
#
# Usage: ./02-delete-symlinks.sh <app_id> ...
#
# Parameters:
# <app_id>: ID of the application to update.

SCRIPT_DIR=${BASH_SOURCE[0]%/*}
. ${SCRIPT_DIR}/../../utils.sh

print_header "${BASH_SOURCE[0]}"
check_args_minimum_number "${#}" 1 "<app_id>"
delete_symbolic_links $1
exit 0
