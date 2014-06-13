#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script contains function utilities.

. ${BASH_SOURCE[0]%/*}/config.sh

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

print_header ()
{
    message=$1

    debug "Running: '${message}'"
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

# Deletes the downloaded file, its sha256file and GPG signature.
# Receives the path to the downloaded file, the name of the sha256file
# and the name of the GPG signature.
delete_download ()
{
    file=$1
    sha256file=$2
    gpgfile=$3

    DIR=${file%/*}
    rm --force "${file}" "${DIR}/${sha256file}" "${DIR}/${gpgfile}"
}

# Desktop updates
# ---------------
# Refreshes the system metadata: compiles the settings XML schema files,
# updates the icon theme cache and the cache database of
# the MIME types handled by the .desktop files.
desktop_updates ()
{
    glib-compile-schemas $OS_GSETTINGS_DIR
    gtk-update-icon-cache --ignore-theme-index $OS_DESKTOP_ICONS_DIR
    update-desktop-database $OS_DESKTOP_FILES_DIR
}

# Symbolic links
# --------------

# Internal function
# Creates symbolic links to all the files contained in the directory.
symbolic_links () {
    target_dir=$1
    links_dir=$2

    if [ -d "${target_dir}" ]; then
        for file in `ls ${target_dir}`; do
            ln --symbolic "${target_dir}/${file}" "${links_dir}/${file}"
        done
    fi
}

# Creates symbolic links, on common OS directories, for the application
# metadata files: .desktop files, desktop icons, gsettings and D-Bus
# services.
# It makes the assumption that the application is installed in
# {EAM_PREFIX}/${appid}.
create_symbolic_links ()
{
    appid=$1

    symbolic_links "${EAM_PREFIX}/${appid}/${APP_DESKTOP_FILES_SUBDIR}" "${OS_DESKTOP_FILES_DIR}"
    symbolic_links "${EAM_PREFIX}/${appid}/${APP_DESKTOP_ICONS_SUBDIR}" "${OS_DESKTOP_ICONS_DIR}"
    symbolic_links "${EAM_PREFIX}/${appid}/${APP_DBUS_SERVICES_SUBDIR}" "${OS_DBUS_SERVICES_DIR}"
    symbolic_links "${EAM_PREFIX}/${appid}/${APP_GSETTINGS_SUBDIR}" "${OS_GSETTINGS_DIR}"
}

# Internal
# Deletes all the symbolic links present the links directory that point to files in the target
# directory.
symbolic_links_delete ()
{
    target_dir=$1
    links_dir=$2

    for file in `ls ${links_dir}`
    do
        if [ -L "${links_dir}/${file}" ]; then
            if [ "${links_dir}/${file}" -ef "${target_dir}/${file}" ]; then
                rm "${links_dir}/${file}"
            fi
        fi
    done
}

# Deletes symbolic links that point to the application's metadata files.
# It makes the assumption that the application is installed in
# {EAM_PREFIX}/${appid}.
delete_symbolic_links ()
{
    appid=$1

    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_DESKTOP_FILES_SUBDIR}" "${OS_DESKTOP_FILES_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_DESKTOP_ICONS_SUBDIR}" "${OS_DESKTOP_ICONS_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_DBUS_SERVICES_SUBDIR}" "${OS_DBUS_SERVICES_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_GSETTINGS_SUBDIR}" "${OS_GSETTINGS_DIR}"
}


# 'tar' utilities
# ---------------
# Extracts files from a .tar file to the given directory.
extract_file_to ()
{
    file=$1
    dir=$2

    tar --no-same-owner --extract --file="${file}" --directory="${dir}"
}

# Arguments
# ---------
# Checks the minimun number of arguments required
check_args_minimum_number ()
{
    n_args=$1
    min_n_args=$2
    args_desc=$3

    if [ "${n_args}" -lt "${min_n_args}" ]; then
        exit_error "Usage: `basename $0` ${args_desc} ..."
    fi
}

# Directories
# -----------
# Deletes a directory recursively. Nonexistent directories are ignored
# (no error is raised).
delete_dir ()
{
    dir=$1

    rm --recursive --force "${dir}"
}
