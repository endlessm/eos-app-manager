#!/bin/bash

# Copyright 2014 Endless Mobile, Inc.
#
# This script contains log utilities.
# It is meant to be used by other scripts that want to print
# error, debug messages, and others.

exit_error() {
    message=$1
    echo "Error: ${message}"
    exit 255
}

warning(){
    message=$1
    echo "Debug: ${message}"
}