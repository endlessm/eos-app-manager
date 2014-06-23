#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script verifies the downloaded EndlessOS Bundle and checks its
# integrity, extracts the bundle into a temporary file and finally moves
# it to its corresponding installation directory.
#
# Usage:
#
# $ ./20-external.sh <app_id> 
#
# Parameters:
# <app_id>: ID of the application to install.
#
# Returns 0 on success.

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"
check_args_minimum_number "${#}" 1 "<app_id>"
APP_ID=$1

parse_ini "${EAM_TMP}/${APP_ID}/.info" "External"

if [ -z "$url" ]; then
    exit 0
fi

output_dir="${EAM_TMP}/${APP_ID}/external"
index=0

mkdir -p ${output_dir}

while [ -n "${url[${index}]}" ] ; do
    external_url=${url[${index}]}
    external_filename=${output_dir}/${filename[${index}]}
    external_sha256sum=${sha256sum[${index}]}

    wget -q ${external_url} -O ${external_filename}
    echo "${external_sha256sum} ${external_filename}" | sha256sum --quiet --status --check
    ((++index))
done

