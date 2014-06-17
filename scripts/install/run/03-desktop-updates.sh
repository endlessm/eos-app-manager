#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script refreshes the system metadata after installing
# an application: compiles the settings XML schema files,
# updates the icon theme cache and the cache database of
# the MIME types handled by the .desktop files.
#
# Usage: ./03-desktop-updates.sh ...

. ${BASH_SOURCE[0]%/*}/../../utils.sh

print_header "${BASH_SOURCE[0]}"
desktop_updates
