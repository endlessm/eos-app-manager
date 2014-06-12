#!/bin/bash

# Copyright 2014 Endless Mobile, Inc.
#
# This script creates symbolic links, on common OS
# directories, for the following application metadata
# files: the .desktop files, desktop icons,
# gsettings files and D-Bus service files.
#
# Usage: ./05-symlinks.sh <app_id> ...
#
# Parameters:
# <app_id>: ID of the application.
#
# IMPORTANT: This script makes some assumptions that could
# be subject of modification:
# - The application is installed in ${PREFIX}/<app_id>, i.e
#   the APP_PREFIX.
# - The application metadata lives in the directories:
#   * ${APP_PREFIX}/${APP_DESKTOP_FILES_SUBDIR} (.desktop files).
#   * ${APP_PREFIX}/${APP_DESKTOP_ICONS_SUBDIR} (desktop icons)
#   * ${APP_PREFIX}/${APP_GSETTINGS_SUBDIR} (gschema.xml files)
#   * ${APP_PREFIX}/${APP_DBUS_SERVICES_SUBDIR} (D-Bus services)
# - Some of the directories listed above might not exist.
#   If they exist, each of them might contain more than one
#   metadata file and symbolic links to all the files
#   contained in the directory will be created.
# - The OS looks for the applications metadata in the
#   directories:
#   * ${OS_DESKTOP_FILES_DIR} (.desktop files).
#   * ${OS_DESKTOP_ICONS_DIR} (desktop icons)
#   * ${OS_GSETTINGS_DIR} (gschema.xml files)
#   * ${OS_DBUS_SERVICES_DIR} (D-Bus services)
#  The symbolic links to each application metadata will be
#  created in those directories.

SCRIPT_DIR=${BASH_SOURCE[0]%/*}
. ${SCRIPT_DIR}/../../install-config.sh
. ${SCRIPT_DIR}/../../utils.sh

debug "Running '${BASH_SOURCE[0]}'"

LN=$(which ln)   || exit_error "Can't find ln"

ARGS=1
if [ $# -lt "$ARGS" ]
then
  exit_error "Usage: `basename $0` <app_id> .."
fi

APP_ID=$1
APP_PREFIX="${PREFIX}/${APP_ID}"

symbolic_links() {
    target_dir=$1; links_dir=$2

    if [ -d "${target_dir}" ]; then
        for file in `ls ${target_dir}`
        do
            $LN --symbolic "${target_dir}/${file}" "${links_dir}/${file}"
            if [ "$?" -ne 0 ]; then
                exit_error "Failed to create symbolic link '${links_dir}/${file}' to '${target_dir}/${file}'"
            fi
        done
    fi
}

# Symbolic links for .desktop files
symbolic_links "${APP_PREFIX}/${APP_DESKTOP_FILES_SUBDIR}" "${OS_DESKTOP_FILES_DIR}"

# Symbolic links for desktop icons
symbolic_links "${APP_PREFIX}/${APP_DESKTOP_ICONS_SUBDIR}" "${OS_DESKTOP_ICONS_DIR}"

# Symbolic links for GSettings
symbolic_links "${APP_PREFIX}/${APP_GSETTINGS_SUBDIR}" "${OS_GSETTINGS_DIR}"

# Symbolic links for DBUS services
symbolic_links "${APP_PREFIX}/${APP_DBUS_SERVICES_SUBDIR}" "${OS_DBUS_SERVICES_DIR}"

exit 0
