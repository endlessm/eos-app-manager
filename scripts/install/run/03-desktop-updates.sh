#!/bin/bash

# Copyright 2014 Endless Mobile, Inc.
#
# This script refreshes the system metadata after installing
# an application: compiles the settings XML schema files,
# updates the icon theme cache and the cache database of
# the MIME types handled by the .desktop files.
#
# Usage: ./03-desktop-updates.sh ...

# This script uses the configuration variables defined in
# configure.sh:
#  * OS_GSETTINGS_DIR
#  * OS_DESKTOP_ICONS_DIR
#  * OS_DESKTOP_FILES_DIR

SCRIPT_DIR=${BASH_SOURCE[0]%/*}

# Include configuration variables
. ${SCRIPT_DIR}/../../install-config.sh

# # Include utilities
. ${SCRIPT_DIR}/../../utils.sh

debug "Running '${BASH_SOURCE[0]}'"

GLIB_COMPILE_SCHEMAS=$(which glib-compile-schemas) || exit_error "Can't find glib-compile-schemas"
GTK_UPDATE_ICON_CACHE=$(which gtk-update-icon-cache) || exit_error "Can't find gtk-update-icon-cache"
UPDATE_DESKTOP_DATABASE=$(which update-desktop-database) || exit_error "Can't find update-desktop-database"

# Compile the settings XML schema files.
$GLIB_COMPILE_SCHEMAS $OS_GSETTINGS_DIR
if [ "$?" -ne 0 ]; then
  warning "glib-compile-schemas '${OS_GSETTINGS_DIR}' failed"
fi

# Update the icon theme cache.
# The directory $OS_DESKTOP_ICONS_DIR must contain a
# .index.theme file.
$GTK_UPDATE_ICON_CACHE -t $OS_DESKTOP_ICONS_DIR
if [ "$?" -ne 0 ]; then
  warning "gtk-update-icon-cache '${OS_DESKTOP_ICONS_DIR}' failed"
fi

# Update the cache database of the MIME types handled by
# desktop files.
$UPDATE_DESKTOP_DATABASE $OS_DESKTOP_FILES_DIR
if [ "$?" -ne 0 ]; then
  warning "update-desktop-database '${OS_DESKTOP_FILES_DIR}' failed"
fi

exit 0
