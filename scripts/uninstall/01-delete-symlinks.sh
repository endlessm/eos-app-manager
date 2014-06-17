#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script deletes symbolic links that point
# to the application metadata files like .desktop files,
# desktop icons, gsettings files and D-Bus service files.
#
# Usage: ./02-delete-symlinks.sh <app_id> ...
#
# Parameters:
# <app_id>: ID of the application to uninstall.

. ${BASH_SOURCE[0]%/*}/../utils.sh

print_header "${BASH_SOURCE[0]}"
check_args_minimum_number "${#}" 1 "<app_id>"
delete_symbolic_links $1
