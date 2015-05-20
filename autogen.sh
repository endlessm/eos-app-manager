#!/bin/sh

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

olddir=`pwd`
cd "$srcdir"

intltoolize --force --copy || exit $?
autoreconf --install --force --verbose || exit $?

cd "$olddir"

test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
