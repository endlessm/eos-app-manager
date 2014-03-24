#!/bin/sh

# Copyright 2014 Endless Mobile, Inc.
#
# This script converts a debian package (deb) into an EndlessOS Bundle
#
# Example:
#
# $ deb2bundle.sh package.deb

failure_exit() 
{
    echo $1 
    exit 255
}

TMPDIR=/var/tmp

DPKGDEB=$(which dpkg-deb) || failure_exit "Can't find dpkg-deb"
MKTEMP=$(which mktemp) || failure_exit "Can't find mktemp"
TAR=$(which tar) || failure_exit "Can't find tar"
RM=$(which rm) || failure_exit "Can't find rm"
TR=$(which tr) || failure_exit "Can't find tr"

DEB=$1
if [ ! -f $DEB ]; then
    failure_exit "Missing debian package"
fi

WDIR=$(${MKTEMP} -d)
if [ ! -d $WDIR ]; then
    failure_exit "Could not create working directory" 
fi

# get the tarball name
PACKAGE=$(${DPKGDEB} --show --showformat "\${Package}" ${DEB})
if [ -z $PACKAGE ]; then
    failure_exit "Can't query debian package"
fi

ARCHIVE=$(${DPKGDEB} --show --showformat "\${Package}.tar.gz" ${DEB})

# extract
mkdir ${WDIR}/${PACKAGE} || failure_exit -1
${DPKGDEB} --fsys-tarfile ${DEB} | \
    ${TAR} --directory ${WDIR}/${PACKAGE} --extract || failure_exit -1

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
