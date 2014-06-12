#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script creates symbolic links, on common OS
# directories, for the application metadata files.
#
# Usage: ./05-symlinks.sh <app_id> ...
#
# Parameters:
# <app_id>: ID of the application.

SCRIPT_DIR=${BASH_SOURCE[0]%/*}
. ${SCRIPT_DIR}/../../utils.sh

debug "Running '${BASH_SOURCE[0]}'"

ARGS=1
if [ $# -lt "$ARGS" ]
then
  exit_error "Usage: `basename $0` <app_id> .."
fi

create_symbolic_links $1
exit 0
