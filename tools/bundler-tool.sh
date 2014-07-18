#!/bin/bash -e

export EAM_PREFIX=/var/tmp/endless
export EAM_TMP=/var/tmp

. ${BASH_SOURCE[0]%/*}/../scripts/utils.sh

install () {
    appid=$1
    bundle=$2

    [ -f "$bundle" ] || exit "$bundle is not a file"
    create_os_directories
    extract_file_to_dir "${bundle}" "${EAM_TMP}"
    mv "${EAM_TMP}/${appid}" "${EAM_PREFIX}"
    create_symbolic_links $1
}

uninstall () {
    appid=$1

    create_os_directories
    delete_symbolic_links ${appid}
    rm --recursive "${EAM_PREFIX}/${appid}"
}

if [ $# -lt 2 ]; then
    echo "Usage: $0 command <appid> [<file>]"
    exit
fi

case "$1" in
    install) install ${2} ${3}
	;;
    uninstall) uninstall ${2}
	;;
    *) echo "Invalid command \"${1}\""
	;;
esac
