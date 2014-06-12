#!/bin/bash

# Copyright 2014 Endless Mobile, Inc.
#
# This script sets some variables used by other scripts
# in this directory. The scripts that need to use any of
# these defined variables should include this one.

# Root directory for installing applications.
# If PREFIX is not set (for example as an environment variable),
# set it to a default value.
if [ -z ${PREFIX+x} ]; then
    PREFIX="/endless"
fi

# Temporary directory where the Endless OS bundles are
# extracted.
# If TMP is not set (as an environment variable), set it to a
# default value.
if [ -z ${TMP+x} ]; then
    TMP="/var/tmp"
fi

# Directory where the Endless OS GPG's keyring is stored.
#
# If EAM_GPGDIR is not set, it is set to a default value.
if [ -z ${EAM_GPGDIR+x} ]; then
    EAM_GPGDIR="/var/lib/eos-app-manager/keyring"
fi

# Subdirectories in each application's installation directory,
# that contain certain metadata files used by the OS to
# launch the application and identify the services it offers,
# among other things.
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
# Desktop files
OS_DESKTOP_FILES_DIR="${PREFIX}/share/applications"
# Desktop icons directories.
OS_DESKTOP_ICONS_DIR="${PREFIX}/share/icons/EndlessOS"
# XML schemas directory.
OS_GSETTINGS_DIR="${PREFIX}/share/glib-2.0/schemas"
# D-Bus services directory.
OS_DBUS_SERVICES_DIR="${PREFIX}/share/dbus-1/services"


# Prints the value of the configuration variables
print_config ()
{
    echo "Scripts configuration"
    echo "---------------------"
    echo "PREFIX=$PREFIX"
    echo "TMP=$TMP"
    echo "APP_DESKTOP_FILES_SUBDIR=$APP_DESKTOP_FILES_SUBDIR"
    echo "APP_DESKTOP_ICONS_SUBDIR=$APP_DESKTOP_ICONS_SUBDIR"
    echo "APP_GSETTINGS_SUBDIR=$APP_GSETTINGS_SUBDIR"
    echo "APP_DBUS_SERVICES_SUBDIR=$APP_DBUS_SERVICES_SUBDIR"
    echo "OS_GSETTINGS_DIR=$OS_GSETTINGS_DIR"
    echo "OS_DESKTOP_ICONS_DIR=$OS_DESKTOP_ICONS_DIR"
    echo "OS_DESKTOP_FILES_DIR=$OS_DESKTOP_FILES_DIR"
}
