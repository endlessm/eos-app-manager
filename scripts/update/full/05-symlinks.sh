#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script creates symbolic links, on common OS
# directories, for the application metadata files.
#
# Usage: ./05-symlinks.sh <app_id> ...
#
# Parameters:
# <app_id>: ID of the application.

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"
check_args_minimum_number "${#}" 1 "<app_id>"
create_symbolic_links $1
