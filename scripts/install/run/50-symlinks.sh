#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script creates symbolic links, on common OS
# directories, for the application metadata files.
#
# Usage: ./50-symlinks.sh <app_id> ...
#
# Parameters:
# <app_id>: ID of the application to install.

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"
check_args_minimum_number "${#}" 1 "<app_id> <bundle_path>"
create_symbolic_links $1