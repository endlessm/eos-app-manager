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
INFO="${EAM_TMP}/${APP_ID}/.info"

output_dir="${EAM_TMP}/${APP_ID}/external"
mkdir -p ${output_dir}

parse_ini_get_sections ${INFO}

for section in "${sections[@]}" ; do
    if  [[ $section != External* ]] ; then
        continue
    fi

    parse_ini_section ${INFO} "${section}"
    output_file=${output_dir}/${filename}

    wget -q ${url} -O ${output_file}
    echo "${sha256sum} ${output_file}" | sha256sum --quiet --status --check
done
