#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script contains function utilities.

SCRIPT_DIR=${BASH_SOURCE[0]%/*}
. ${SCRIPT_DIR}/config.sh

# Log
# ---
# Print errors, debug messages, and others.
exit_error ()
{
    message=$1

    printf "Error: ${message}\n"
    exit 255
}

warning ()
{
    message=$1

    printf "Warning: ${message}\n"
}

debug ()
{
    message=$1

    printf "Debug: ${message}\n"
}

# Downloaded files
# ----------------
# The functions under this section assume that the sha256 file and the
# GPG signature are in the same directory than the downloaded file.

# Verifies and checks the integrity of a downloaded file. Receives the
# path to the downloaded file, the name of sha256file and the name of
# the GPG signature.
verify_download ()
{
    file=$1
    sha256file=$2
    gpgfile=$3

    DIR=${file%/*}
    cd $DIR

    sha256sum --quiet --status --check "${sha256file}"
    gpg --homedir="${EAM_GPGDIR}" --quiet --verify "${gpgfile}" "${file}"
}
