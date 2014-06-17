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
verify_downloaded_file ()
{
    if [ "$#" -ne 3 ]; then
        exit_error "verify_downloaded_file: incorrect number of arguments"
    fi

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
delete_downloaded_file ()
{
    if [ "$#" -ne 3 ]; then
        exit_error "delete_downloaded_file: incorrect number of arguments"
    fi

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
# Creates symbolic links to all the files contained in the directory
symbolic_links ()
{
    if [ "$#" -ne 2 ]; then
        exit_error "symbolic_links: incorrect number of arguments"
    fi

    target_dir=$1
    links_dir=$2

    if [ -d "${target_dir}" ]; then
        find "${target_dir}" -mindepth 1 -exec ln --symbolic '{}' "${links_dir}" \;
    fi
}

# Creates symbolic links, on common OS directories, for the application
# metadata files: .desktop files, desktop icons, gsettings and D-Bus
# services.
# It makes the assumption that the application is installed in
# {EAM_PREFIX}/${appid}.
create_symbolic_links ()
{
    if [ "$#" -ne 1 ]; then
        exit_error "create_symbolic_links: incorrect number of arguments"
    fi

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
    if [ "$#" -ne 2 ]; then
        exit_error "symbolic_links_delete: incorrect number of arguments"
    fi

    target_dir=$1
    links_dir=$2

    if [ -d "${target_dir}" ]; then
        find  "${target_dir}" -mindepth 1 -print0 | xargs -0 -I '{.}' find -L "${links_dir}" -samefile '{.}' -exec rm '{}' \;
    fi
}

# Deletes symbolic links that point to the application's metadata files.
# It makes the assumption that the application is installed in
# {EAM_PREFIX}/${appid}.
delete_symbolic_links ()
{
    if [ "$#" -ne 1 ]; then
        exit_error "delete_symbolic_links: incorrect number of arguments"
    fi

    appid=$1

    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_DESKTOP_FILES_SUBDIR}" "${OS_DESKTOP_FILES_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_DESKTOP_ICONS_SUBDIR}" "${OS_DESKTOP_ICONS_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_DBUS_SERVICES_SUBDIR}" "${OS_DBUS_SERVICES_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_GSETTINGS_SUBDIR}" "${OS_GSETTINGS_DIR}"
}


# 'tar' utilities
# ---------------
# Extracts files from a .tar file to the given directory.
extract_file_to_dir ()
{
    if [ "$#" -ne 2 ]; then
        exit_error "extract_file_to_dir: incorrect number of arguments"
    fi

    file=$1
    dir=$2

    tar --no-same-owner --extract --file="${file}" --directory="${dir}"
}

# Arguments
# ---------
# Checks the minimun number of arguments required
check_args_minimum_number ()
{
    if [ "$#" -ne 3 ]; then
        exit_error "check_args_minimum_number: incorrect number of arguments"
    fi

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
    if [ "$#" -ne 1 ]; then
        exit_error "delete_dir: incorrect number of arguments"
    fi

    dir=$1

    rm --recursive --force "${dir}"
}
