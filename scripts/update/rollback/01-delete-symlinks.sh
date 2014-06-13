#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script deletes symbolic links that point
# to the application's metadata files.

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"
check_args_minimum_number "${#}" 1 "<app_id>"
delete_symbolic_links $1
