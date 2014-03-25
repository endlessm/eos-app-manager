#!/bin/sh

GDBUS=`which gdbus`
PARAMS="--session --dest com.Endless.AppManager --object-path /com/Endless/AppManager"

call() {
    method=$1
    ${GDBUS} call ${PARAMS} --method com.Endless.AppManager.${method}
}

introspect() {
    ${GDBUS} introspect ${PARAMS}
}

introspect
call "Refresh"
