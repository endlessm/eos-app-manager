#!/bin/sh

GDBUS=`which gdbus`
PARAMS="--system --dest com.Endless.AppManager --object-path /com/Endless/AppManager"

call() {
    method=$1; shift
    ${GDBUS} call ${PARAMS} --method com.Endless.AppManager.${method} $*
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
    list) call "ListAvailable"
	;;
    quit) call "Quit"
	;;
    *) echo "invalid command \"$1\""
	;;
esac
