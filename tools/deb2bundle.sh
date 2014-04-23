#!/bin/bash -e

# Copyright 2014 Endless Mobile, Inc.
#
# This script converts a debian package (deb) into an EndlessOS Bundle
#
# Example:
#
# $ deb2bundle.sh package.deb

extract_meta() {
  \dpkg-deb -f $DEBIAN_PKG $1
}

if [ $# -ne 1 ]; then
  echo "Usage: $0 <debian package>"
  exit 1
fi

DEBIAN_PKG=$1
if [ ! -f $DEBIAN_PKG ]; then
  echo "File ($1) not found"
  exit 1
fi

PACKAGE=$(dpkg-deb -f $DEBIAN_PKG Package)
if [ -z $PACKAGE ]; then
  echo "$1 is not a Debian package"
  exit 1
fi

WORKING_DIR=$(mktemp -d)
echo "Using $WORKING_DIR as temp dir"

VERSION=$(extract_meta Version)
DESCRIPTION=$(extract_meta Description | tr '\n' '.')
HOMEPAGE=$(extract_meta Homepage)
ARCH=$(extract_meta Architecture)

if [[ "${PACKAGE}" =~ ^eos-* ]]; then
  PACKAGE="com.endlessm.${PACKAGE##eos-}"
  echo "WARN: Package name changed to $PACKAGE"
fi

PKG_EXTRACTION_DIR=$WORKING_DIR/$PACKAGE
\mkdir $PKG_EXTRACTION_DIR

ARCHIVE="${PACKAGE}_${VERSION}_${ARCH}"
echo "$ARCHIVE"
echo "  $HOMEPAGE"
echo "  $DESCRIPTION"

\dpkg --extract $DEBIAN_PKG $PKG_EXTRACTION_DIR

\mv $PKG_EXTRACTION_DIR/usr/* $PKG_EXTRACTION_DIR
\rm -rf $PKG_EXTRACTION_DIR/usr

\cat > $PKG_EXTRACTION_DIR/.info <<EOF
[Bundle]
appid=${PACKAGE}
version=${VERSION}
homepage=${HOMEPAGE}
architecture=${ARCH}
description=${DESCRIPTION}
EOF

\tar --directory $WORKING_DIR --create --gzip --file ./$ARCHIVE\.bundle $PACKAGE

\rm -rf $WORKING_DIR
