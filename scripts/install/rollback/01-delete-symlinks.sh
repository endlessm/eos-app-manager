#!/bin/bash

# Copyright 2014 Endless Mobile, Inc.
#
# This script deletes symbolic links that point
# to the application metadata files like .desktop files,
# desktop icons, gsettings files and D-Bus service files.
#
# Usage: ./01-delete-symlinks.sh <app_id> ...
#
# Parameters:
# <app_id>: ID of the application which installation failed.
#
# IMPORTANT: This script makes some assumptions that could
# be subject of modification:
# - The application is installed in ${PREFIX}/<app_id>, i.e
# the APP_PREFIX.
# - The application metadata lives in the directories:
#   * ${APP_PREFIX}/${APP_DESKTOP_FILES_SUBDIR} (.desktop files).
#   * ${APP_PREFIX}/${APP_DESKTOP_ICONS_SUBDIR} (desktop icons)
#   * ${APP_PREFIX}/${APP_GSETTINGS_SUBDIR} (gschema.xml files)
#   * ${APP_PREFIX}/${APP_DBUS_SERVICES_SUBDIR} (D-Bus services)
# - The OS looks for the applications metadata in the
#   directories:
#   * ${OS_DESKTOP_FILES_DIR} (.desktop files).
#   * ${OS_DESKTOP_ICONS_DIR} (desktop icons)
#   * ${OS_GSETTINGS_DIR} (gschema.xml files)
#   * ${OS_DBUS_SERVICES_DIR} (D-Bus services)
#  The symbolic links to the application metadata present on those
#  directories will be deleted.

SCRIPT_DIR=${BASH_SOURCE[0]%/*}
. ${SCRIPT_DIR}/../../install-config.sh
. ${SCRIPT_DIR}/../../utils.sh

print_installation_config
debug "Running '${BASH_SOURCE[0]}'"

RM=$(which rm)   || exit_error "Can't find rm"

ARGS=1
if [ $# -lt "$ARGS" ]
then
  exit_error "Usage: `basename $0` <app_id> .."
fi

APP_ID=$1
APP_PREFIX="${PREFIX}/${APP_ID}"

delete_symbolic_links() {
    target_dir=$1; links_dir=$2

    for file in `ls ${links_dir}`
    do
        if [ -L "${links_dir}/${file}" ]; then
            if [ "${links_dir}/${file}" -ef "${target_dir}/${file}" ]; then
                $RM "${links_dir}/${file}"
                  if [ "$?" -ne 0 ]; then
                      exit_error "Failed to delete symbolic link '${links_dir}/${file}' to '${target_dir}/${file}'"
                  fi
            fi
        fi
    done
}


# Symbolic links for .desktop files
delete_symbolic_links "${APP_PREFIX}/${APP_DESKTOP_FILES_SUBDIR}" "${OS_DESKTOP_FILES_DIR}"

# Symbolic links for desktop icons
delete_symbolic_links "${APP_PREFIX}/${APP_DESKTOP_ICONS_SUBDIR}" "${OS_DESKTOP_ICONS_DIR}"

# Symbolic links for GSettings
delete_symbolic_links "${APP_PREFIX}/${APP_GSETTINGS_SUBDIR}" "${OS_GSETTINGS_DIR}"

# Symbolic links for DBUS services
delete_symbolic_links "${APP_PREFIX}/${APP_DBUS_SERVICES_SUBDIR}" "${OS_DBUS_SERVICES_DIR}"

exit 0
