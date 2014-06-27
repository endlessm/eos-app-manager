#!/bin/sh

GDBUS=`which gdbus`
PARAMS="--system --dest com.endlessm.AppManager --object-path /com/endlessm/AppManager"

call() {
    method=$1; shift
    ${GDBUS} call ${PARAMS} --method com.endlessm.AppManager.${method} --timeout 1000 $*
}

introspect() {
    ${GDBUS} introspect ${PARAMS}
}

if [ $# -lt 1 ]; then
    echo "Usage : $0 command [appID]"
    exit
fi

case "$1" in
    introspect) introspect
	;;
    refresh) call "Refresh"
	;;
    install) call "Install" "$2"
	;;
    uninstall) call "Uninstall" "$2"
	;;
    list-available) call "ListAvailable"
	;;
    list-installed) call "ListInstalled"
	;;
    caps) call "GetUserCapabilities"
	;;
    quit) call "Quit"
	;;
    cancel) call "Cancel"
	;;
    *) echo "invalid command \"$1\""
	;;
esac
