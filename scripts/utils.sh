#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script contains function utilities.

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
