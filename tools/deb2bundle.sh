#!/bin/sh

# Copyright 2014 Endless Mobile, Inc.
#
# This script converts a debian package (deb) into an EndlessOS Bundle
#
# Example:
#
# $ deb2bundle.sh package.deb


TMPDIR=/var/tmp

DPKGDEB=$(which dpkg-deb) || exit -1
MKTEMP=$(which mktemp) || exit -1
TAR=$(which tar) || exit -1
RM=$(which rm) || exit -1
TR=$(which tr) || exit -1

DEB=$1
if [ ! -f $DEB ]; then
    echo "Missing debian package"
    exit -1
fi

WDIR=$(${MKTEMP} -d)
if [ ! -d $WDIR ]; then
    echo "Could not create working directory"
    exit -1
fi

# get the tarball name
PACKAGE=$(${DPKGDEB} --show --showformat "\${Package}" ${DEB})
if [ -z $PACKAGE ]; then
    echo "Can't query debian package"
    exit -1
fi

ARCHIVE=$(${DPKGDEB} --show --showformat "\${Package}.tar.gz" ${DEB})

# extract
mkdir ${WDIR}/${PACKAGE} || exit -1
${DPKGDEB} --fsys-tarfile ${DEB} | \
    ${TAR} --directory ${WDIR}/${PACKAGE} --extract || exit -1

# craft info file
VERSION=$(${DPKGDEB} --show --showformat "\${Version}" ${DEB})
HOMEPAGE=$(${DPKGDEB} --show --showformat "\${Homepage}" ${DEB})
ARCH=$(${DPKGDEB} --show --showformat "\${Architecture}" ${DEB})
DESC=$(${DPKGDEB} --show --showformat "\${Description}" ${DEB} | ${TR} '\n' '.')

cat > ${WDIR}/.info <<EOF
[Bundle]
appid=${PACKAGE}
version=${VERSION}
homepage=${HOMEPAGE}
architecture=${ARCH}
description=${DESC}
EOF

# output
${TAR} --directory ${WDIR} --create --gzip --file ./${ARCHIVE} ${PACKAGE}

# clean
${RM} -rf ${WDIR}
