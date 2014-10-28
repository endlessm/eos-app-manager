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
    gpgv --keyring="${EAM_GPGKEYRING}" --logger-fd 1 --quiet "${gpgfile}" "${file}"
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

# Paths
# -----

# Internal function
# Removes a directory from a $PATH-style variable
# Second argument is the name of the path variable to be
# modified (default: PATH)
pathremove () {
    if [ "$#" -lt 1 ]; then
        exit_error "pathremove: at least one argument is required"
    fi

    local IFS=':'
    local NEWPATH
    local DIR
    local PATHVARIABLE=${2:-PATH}
    for DIR in ${!PATHVARIABLE} ; do
        if [ "$DIR" != "$1" ] ; then
            NEWPATH=${NEWPATH:+$NEWPATH:}$DIR
        fi
    done
    export $PATHVARIABLE="$NEWPATH"
}

# Internal function
# Prepends a directory to a $PATH-style variable
# Second argument is the name of the path variable to be
# modified (default: PATH)
pathprepend () {
    if [ "$#" -lt 1 ]; then
        exit_error "pathprepend: at least one argument is required"
    fi

    pathremove $1 $2
    local PATHVARIABLE=${2:-PATH}
    export $PATHVARIABLE="$1${!PATHVARIABLE:+:${!PATHVARIABLE}}"
}

# Internal function
# Appends a directory to a $PATH-style variable
# Second argument is the name of the path variable to be
# modified (default: PATH)
pathappend () {
    if [ "$#" -lt 1 ]; then
        exit_error "pathappend: at least one argument is required"
    fi

    pathremove $1 $2
    local PATHVARIABLE=${2:-PATH}
    export $PATHVARIABLE="${!PATHVARIABLE:+${!PATHVARIABLE}:}$1"
}

# Internal function
# Checks if a directory is in a $PATH-style variable
# Second argument is the name of the path variable to be
# modified (default: PATH)
pathexists () {
    if [ "$#" -lt 1 ]; then
        exit_error "pathexists: at least one argument is required"
    fi

    local IFS=':'
    local DIR
    local PATHVARIABLE=${2:-PATH}

    for DIR in ${!PATHVARIABLE}; do
        [ "$DIR" = "$1" ] && return 0
    done

    # no match found
    return 1
}

# Symbolic links
# --------------

# Internal function
# Ensure symbolic links to all the files passed through stdin
# The directory structure will be replicated into the links directory,
# so that only regular files are linked, and their target is the
# same subdirectory
ensure_symbolic_links ()
{
    if [ "$#" -ne 2 ]; then
        exit_error "ensure_symbolic_link: incorrect number of arguments"
    fi

    target_dir=$1
    links_dir=$2

    [ -d "${links_dir}" ] || mkdir --parents "${links_dir}"
    while read -d $'\0' source_file; do
        if [ -f "${source_file}" -o -L "${source_file}" ]; then
            dirname=$(dirname "${source_file}")
            ln --symbolic "${target_dir}"/"${source_file}" "${links_dir}"/"${dirname}"
        elif [ -d "${source_file}" ]; then
            mkdir --parents "${links_dir}"/"${source_file}"
        fi
    done
}

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
        cd "${target_dir}"; find . -mindepth 1 -print0 | ensure_symbolic_links "${target_dir}" "${links_dir}"
    fi
}

# Internal function
# Creates a symbolic link to the binary specified in the desktop file
binary_symbolic_link ()
{
    if [ "$#" -ne 1 ]; then
        exit_error "binary_symbolic_link: incorrect number of arguments"
    fi

    appid=$1

    appdesktopfile="${EAM_PREFIX}/${appid}/${APP_DESKTOP_FILES_SUBDIR}/${appid}.desktop"
    if [ ! -f "${appdesktopfile}" ]; then
        # Use the first one we find as a last resort
        appdesktopfile=$(find "${EAM_PREFIX}/${appid}/${APP_DESKTOP_FILES_SUBDIR}/" -name "*.desktop" | head -n 1)
    fi

    if [ ! -f "${appdesktopfile}" ]; then
        exit_error "binary_symbolic_link: no desktop files in bundle"
    fi

    # Use TryExec if present...
    binaryname=$(grep '^TryExec' "${appdesktopfile}" | cut --delimiter '=' --fields 2)
    if [ -z "${binaryname}" ]; then
	# Otherwise use Exec
	binaryname=$(grep '^Exec' "${appdesktopfile}" | cut --delimiter '=' --fields 2)
	# Strip everything past the first space
	binaryname=${binaryname%% *}
	# If the path is absolute, only get the last component
	binaryname=${binaryname##*/}
    fi

    binpath="${EAM_PREFIX}/${appid}/${APP_BIN_SUBDIR}/${binaryname}"
    gamespath="${EAM_PREFIX}/${appid}/${APP_GAMES_SUBDIR}/${binaryname}"

    if [ -f "${binpath}" ]; then
	# First look in /endless/$appid/bin
	ln --symbolic "${binpath}" "${OS_BIN_DIR}"
    elif [ -f "${gamespath}" ]; then
	# Then look in /endless/$appid/games
	ln --symbolic "${gamespath}" "${OS_BIN_DIR}"
    else
	# Finally, look if the command we are trying to link is already in $PATH
	#
	# We don't want to match binaries in ${OS_BIN_DIR} here, so remove it
	# from $PATH first
	oldpath=$PATH
	pathremove "${OS_BIN_DIR}"

	cmdfound=0
	if command -v "${binaryname}" > /dev/null 2>&1; then
	    cmdfound=1
	fi

	# Restore $PATH
	export PATH="${oldpath}"

	if [ "${cmdfound}" == 0 ]; then
	    # If command is neither in $PATH nor in one of the binary directories
	    # of the bundle, exit with error
	    exit_error "binary_symbolic_link: can't find app binary to link"
	fi
    fi
}

# Internal function
# Creates a symbolic link to the bundle's EKN data
ekn_data_symbolic_link ()
{
    if [ "$#" -ne 1 ]; then
        exit_error "ekn_data_symbolic_link: incorrect number of arguments"
    fi

    appid=$1
    target_dir="${EAM_PREFIX}/${appid}/${APP_EKN_DATA_SUBDIR}"

    # Link only the first level of the EKN data subdir.
    if [ -d "${target_dir}" ]; then
	find "${target_dir}" -mindepth 1 -maxdepth 1 -exec ln --symbolic '{}' "${OS_EKN_DATA_DIR}" \;
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

    binary_symbolic_link "${appid}"

    symbolic_links "${EAM_PREFIX}/${appid}/${APP_DESKTOP_FILES_SUBDIR}" "${OS_DESKTOP_FILES_DIR}"
    symbolic_links "${EAM_PREFIX}/${appid}/${APP_DESKTOP_ICONS_SUBDIR}" "${OS_DESKTOP_ICONS_DIR}"
    symbolic_links "${EAM_PREFIX}/${appid}/${APP_DBUS_SERVICES_SUBDIR}" "${OS_DBUS_SERVICES_DIR}"
    symbolic_links "${EAM_PREFIX}/${appid}/${APP_HELP_SUBDIR}" "${OS_HELP_DIR}"

    ekn_data_symbolic_link "${appid}"

    symbolic_links "${EAM_PREFIX}/${appid}/${APP_EKN_MANIFEST_SUBDIR}" "${OS_EKN_MANIFEST_DIR}"
    symbolic_links "${EAM_PREFIX}/${appid}/${APP_GSETTINGS_SUBDIR}" "${OS_GSETTINGS_DIR}"
    symbolic_links "${EAM_PREFIX}/${appid}/${APP_SHELL_SEARCH_SUBDIR}" "${OS_SHELL_SEARCH_DIR}"
    symbolic_links "${EAM_PREFIX}/${appid}/${APP_KDE_HELP_SUBDIR}" "${OS_KDE_HELP_DIR}"
    symbolic_links "${EAM_PREFIX}/${appid}/${APP_KDE4_SUBDIR}" "${OS_KDE4_DIR}"
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
        find ${links_dir} -mindepth 1 -type l -lname "${target_dir}*" -print0 | xargs -0 rm --force
    fi

    # Try to cleanup empty directories that had links. Use -depth option
    # to handle the contents of the directory first.
    find ${links_dir} -mindepth 1 -depth -type d -exec \
        rmdir --ignore-fail-on-non-empty '{}' ';'
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

    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_BIN_SUBDIR}" "${OS_BIN_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_GAMES_SUBDIR}" "${OS_BIN_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_DESKTOP_FILES_SUBDIR}" "${OS_DESKTOP_FILES_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_DESKTOP_ICONS_SUBDIR}" "${OS_DESKTOP_ICONS_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_DBUS_SERVICES_SUBDIR}" "${OS_DBUS_SERVICES_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_HELP_SUBDIR}" "${OS_HELP_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_EKN_DATA_SUBDIR}" "${OS_EKN_DATA_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_EKN_MANIFEST_SUBDIR}" "${OS_EKN_MANIFEST_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_GSETTINGS_SUBDIR}" "${OS_GSETTINGS_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_SHELL_SEARCH_SUBDIR}" "${OS_SHELL_SEARCH_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_KDE_HELP_SUBDIR}" "${OS_KDE_HELP_DIR}"
    symbolic_links_delete "${EAM_PREFIX}/${appid}/${APP_KDE4_SUBDIR}" "${OS_KDE4_DIR}"
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
# Create the needed directories in EAM_PREFIX. This is needed when the
# scripts are run by the image-builder and ensures a sane setup at app
# installation time and not just at daemon init.
create_os_directories ()
{
    for dir in \
        "${OS_BIN_DIR}" "${OS_DESKTOP_FILES_DIR}" \
        "${OS_DESKTOP_ICONS_DIR}" "${OS_GSETTINGS_DIR}" \
        "${OS_DBUS_SERVICES_DIR}" "${OS_HELP_DIR}" \
        "${OS_EKN_DATA_DIR}" "${OS_EKN_MANIFEST_DIR}" \
        "${OS_SHELL_SEARCH_DIR}" "${OS_KDE_HELP_DIR}" \
        "${OS_KDE4_DIR}"
    do
        [ -d "${dir}" ] || mkdir --parents "${dir}"
    done

    # Older versions of eos-app-manager used gpg, which created a
    # .gnupg directory in EAM_PREFIX. Delete any existing directory
    # as it's unneeded now that gpgv is used.
    delete_dir "${EAM_PREFIX}/.gnupg"
}

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

# .info
#------
# Get an array with all the sections available in a ini-like file
parse_ini_get_sections ()
{
    if [ "$#" -ne 1 ]; then
        exit_error "parse_ini_get_sections: incorrect number of arguments"
    fi

    config_file=$1

    eval sections=(`grep "^\[" ${config_file} | tr '\[' '"' | tr '\]' '"' `)
}

# Parses the section from a ini-like file
parse_ini_section ()
{
    if [ "$#" -ne 2 ]; then
        exit_error "parse_ini_section: incorrect number of arguments"
    fi

    config_file=$1
    section=$2

    eval `sed -e 's/[[:space:]]*\=[[:space:]]*/=/g' \
      -e 's/;.*$//' \
      -e 's/[[:space:]]*$//' \
      -e 's/^[[:space:]]*//' \
      -e "s/^\(.*\)=\([^\"']*\)$/\1=\"\2\"/" \
     < ${config_file} \
      | sed -n -e "/^\[${section}\]/,/^\s*\[/{/^[^;].*\=.*/p;}"`
}

# python modules
#---------------
# Recursively byte compile all python modules in the site directories.
compile_python_modules ()
{
    if [ "$#" -ne 1 ]; then
        exit_error "compile_python_modules: incorrect number of arguments"
    fi

    dir=$1

    for python in python2 python3; do
        for sitedir in dist-packages site-packages; do
            if [ "${python}" = python2 ]; then
                pyvers=$(${python} -c 'import sys; print sys.version[0:3]')
                pydir="${dir}/lib/python${pyvers}/${sitedir}"
            else
                pydir="${dir}/lib/${python}/${sitedir}"
            fi
            if [ -d "${pydir}" ]; then
                ${python} -m compileall -f -q "${pydir}"
            fi
        done
    done
}

# Remove compiled bytecode recursively.
remove_python_bytecode ()
{
    if [ "$#" -ne 1 ]; then
        exit_error "remove_python_bytecode: incorrect number of arguments"
    fi

    dir=$1

    find "${dir}" -type d -name __pycache__ -exec rm -rf '{}' ';'
    find "${dir}" -type f -name '*.py[co]' -delete
}
