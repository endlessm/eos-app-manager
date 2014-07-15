#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script compiles python modules within the EndlessOS Bundle
# installation directory.
#
# Usage:
#
# $ ./50-python-compile.sh <app_id>
#
# Parameters:
# <app_id>: ID of the application to install.
#
# Returns 0 on success.

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"
check_args_minimum_number "${#}" 1 "<app_id>"
APP_ID=$1

compile_python_modules "${EAM_PREFIX}/${APP_ID}"
