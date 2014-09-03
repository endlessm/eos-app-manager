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
# Application games binaries subdirectory.
APP_GAMES_SUBDIR="games"
# Application .desktop file subdirectory.
APP_DESKTOP_FILES_SUBDIR="share/applications"
# Application desktop icons subdirectory.
APP_DESKTOP_ICONS_SUBDIR="share/icons"
# Application XML schemas subdirectory.
APP_GSETTINGS_SUBDIR="share/glib-2.0/schemas"
# Application D-Bus services subdirectory.
APP_DBUS_SERVICES_SUBDIR="share/dbus-1/services"
# Application help subdirectory
APP_HELP_SUBDIR="share/help"
# Application knowledge app data.
APP_EKN_DATA_SUBDIR="share/ekn/data"
# Application knowledge app manifest.
APP_EKN_MANIFEST_SUBDIR="share/ekn/manifest"
# Application shell search provider directory.
APP_SHELL_SEARCH_SUBDIR="share/gnome-shell/search-providers"

# Directories where the OS looks for the applications metadata.
# These directories typically contain symbolic links to the
# metadata files in the installation directory of every
# application.
# Binaries (including games)
OS_BIN_DIR="${EAM_PREFIX}/bin"
# Desktop files
OS_DESKTOP_FILES_DIR="${EAM_PREFIX}/share/applications"
# Desktop icons directories.
OS_DESKTOP_ICONS_DIR="${EAM_PREFIX}/share/icons"
# XML schemas directory.
OS_GSETTINGS_DIR="${EAM_PREFIX}/share/glib-2.0/schemas"
# D-Bus services directory.
OS_DBUS_SERVICES_DIR="${EAM_PREFIX}/share/dbus-1/services"
# Help directory
OS_HELP_DIR="${EAM_PREFIX}/share/help"
# Knowledge app data.
OS_EKN_DATA_DIR="${EAM_PREFIX}/share/ekn/data"
# Knowledge app manifest.
OS_EKN_MANIFEST_DIR="${EAM_PREFIX}/share/ekn/manifest"
# Shell search provider directory.
OS_SHELL_SEARCH_DIR="${EAM_PREFIX}/share/gnome-shell/search-providers"


# Prints the value of the configuration variables
print_config ()
{
    echo "Scripts configuration"
    echo "---------------------"
    echo "EAM_PREFIX=$EAM_PREFIX"
    echo "EAM_TMP=$EAM_TMP"
    echo "EAM_GPGKEYRING=$EAM_GPGKEYRING"
    echo "APP_BIN_SUBDIR=$APP_BIN_SUBDIR"
    echo "APP_GAMES_SUBDIR=$APP_GAMES_SUBDIR"
    echo "APP_DESKTOP_FILES_SUBDIR=$APP_DESKTOP_FILES_SUBDIR"
    echo "APP_DESKTOP_ICONS_SUBDIR=$APP_DESKTOP_ICONS_SUBDIR"
    echo "APP_GSETTINGS_SUBDIR=$APP_GSETTINGS_SUBDIR"
    echo "APP_DBUS_SERVICES_SUBDIR=$APP_DBUS_SERVICES_SUBDIR"
    echo "APP_EKN_DATA_SUBDIR=$APP_EKN_DATA_SUBDIR"
    echo "APP_EKN_MANIFEST_SUBDIR=$APP_EKN_MANIFEST_SUBDIR"
    echo "APP_SHELL_SEARCH_SUBDIR=$APP_SHELL_SEARCH_SUBDIR"
    echo "OS_BIN_DIR=$OS_BIN_DIR"
    echo "OS_DESKTOP_FILES_DIR=$OS_DESKTOP_FILES_DIR"
    echo "OS_DESKTOP_ICONS_DIR=$OS_DESKTOP_ICONS_DIR"
    echo "OS_GSETTINGS_DIR=$OS_GSETTINGS_DIR"
    echo "OS_DBUS_SERVICES_DIR=$OS_DBUS_SERVICES_DIR"
    echo "OS_EKN_DATA_DIR=$OS_EKN_DATA_DIR"
    echo "OS_EKN_MANIFEST_DIR=$OS_EKN_MANIFEST_DIR"
    echo "OS_SHELL_SEARCH_DIR=$OS_SHELL_SEARCH_DIR"
}
