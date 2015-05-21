#!/bin/sh

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

olddir=`pwd`
cd "$srcdir"

autoreconf --install --force --verbose || exit $?

cd "$olddir"

test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
