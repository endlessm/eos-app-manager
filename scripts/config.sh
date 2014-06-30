#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script sets some variables used by other scripts
# in this directory. The scripts that need to use any of
# these defined variables should include this one.

# Root directory for installing applications.
# If EAM_PREFIX is not set (for example as an environment variable),
# set it to a default value.
if [ -z ${EAM_PREFIX+x} ]; then
    EAM_PREFIX="/endless"
fi

# Temporary directory where the Endless OS bundles are
# extracted.
# If EAM_TMP is not set (as an environment variable), set it to a
# default value.
if [ -z ${EAM_TMP+x} ]; then
    EAM_TMP="/var/tmp"
fi

# Directory where the Endless OS GPG's keyring is stored.
#
# If EAM_GPGKEYRING is not set, it is set to a default value.
if [ -z ${EAM_GPGKEYRING+x} ]; then
    EAM_GPGKEYRING="/usr/share/eos-app-manager/eos-keyring.gpg"
fi

# Subdirectories in each application's installation directory,
# that contain certain metadata files used by the OS to
# launch the application and identify the services it offers,
# among other things.
# Application binaries subdirectory.
APP_BIN_SUBDIR="bin"
# Application .desktop file subdirectory.
APP_DESKTOP_FILES_SUBDIR="share/applications"
# Application desktop icons subdirectory.
APP_DESKTOP_ICONS_SUBDIR="share/icons/EndlessOS"
# Application XML schemas subdirectory.
APP_GSETTINGS_SUBDIR="share/glib-2.0/schemas"
# Applications D-Bus services subdirectory.
APP_DBUS_SERVICES_SUBDIR="share/dbus-1/services"

# Directories where the OS looks for the applications metadata.
# These directories typically contain symbolic links to the
# metadata files in the installation directory of every
# application.
# Binaries
OS_BIN_DIR="${EAM_PREFIX}/bin"
# Desktop files
OS_DESKTOP_FILES_DIR="${EAM_PREFIX}/share/applications"
# Desktop icons directories.
OS_DESKTOP_ICONS_DIR="${EAM_PREFIX}/share/icons/EndlessOS"
# XML schemas directory.
OS_GSETTINGS_DIR="${EAM_PREFIX}/share/glib-2.0/schemas"
# D-Bus services directory.
OS_DBUS_SERVICES_DIR="${EAM_PREFIX}/share/dbus-1/services"


# Prints the value of the configuration variables
print_config ()
{
    echo "Scripts configuration"
    echo "---------------------"
    echo "EAM_PREFIX=$EAM_PREFIX"
    echo "EAM_TMP=$EAM_TMP"
    echo "APP_DESKTOP_FILES_SUBDIR=$APP_DESKTOP_FILES_SUBDIR"
    echo "APP_DESKTOP_ICONS_SUBDIR=$APP_DESKTOP_ICONS_SUBDIR"
    echo "APP_GSETTINGS_SUBDIR=$APP_GSETTINGS_SUBDIR"
    echo "APP_DBUS_SERVICES_SUBDIR=$APP_DBUS_SERVICES_SUBDIR"
    echo "OS_GSETTINGS_DIR=$OS_GSETTINGS_DIR"
    echo "OS_DESKTOP_ICONS_DIR=$OS_DESKTOP_ICONS_DIR"
    echo "OS_DESKTOP_FILES_DIR=$OS_DESKTOP_FILES_DIR"
}
