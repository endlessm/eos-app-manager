#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script runs the custom installer provided by the bundle.
#
# Usage:
#
# $ ./30-custom-installer.sh <app_id>
#
# Parameters:
# <app_id>: ID of the application to install.
#
# Returns 0 on success.

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"
check_args_minimum_number "${#}" 1 "<app_id>"
APP_ID=$1
WORKPATH=${EAM_TMP}/${APP_ID}

if [ -x ${WORKPATH}/.script.install ] ; then
    cd ${WORKPATH} && ${WORKPATH}/.script.install ${APP_ID} ${WORKPATH}
fi
