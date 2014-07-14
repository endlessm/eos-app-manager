#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script deletes compiled python bytecode within the EndlessOS Bundle
# installation directory.
#
# Usage:
#
# $ ./15-python-cleanup.sh <app_id>
#
# Parameters:
# <app_id>: ID of the application to install.
#
# Returns 0 on success.

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"
check_args_minimum_number "${#}" 1 "<app_id>"
APP_ID=$1

remove_python_bytecode "${EAM_PREFIX}/${APP_ID}"
