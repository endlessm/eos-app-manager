#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script creates the necessary directories in EAM_PREFIX in case
# the daemon hasn't created them for us.
#
# Usage: ./00-directories.sh ...

. ${BASH_SOURCE[0]%/*}/../utils.sh

print_header "${BASH_SOURCE[0]}"
create_os_directories
