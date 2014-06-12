#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script deletes symbolic links that point
# to the application's metadata files.
#
# Usage: ./02-delete-symlinks.sh <app_id> ...
#
# Parameters:
# <app_id>: ID of the application to update.

SCRIPT_DIR=${BASH_SOURCE[0]%/*}
. ${SCRIPT_DIR}/../../utils.sh

debug "Running '${BASH_SOURCE[0]}'"

ARGS=1
if [ $# -lt "$ARGS" ]
then
  exit_error "Usage: `basename $0` <app_id> .."
fi

delete_symbolic_links $1
exit 0
