#!/bin/sh

intltoolize --force --copy
autoreconf --install
./configure $*
