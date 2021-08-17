#!/bin/bash

set -e

LIBMNL_SOURCE_DIR=$1
LIBMNL_INSTALL_DIR=$2/mnl
CONFIG_HOST=$3

echo "MNL lib source dir: ${LIBMNL_SOURCE_DIR}"
echo "MNL lib install dir: ${LIBMNL_INSTALL_DIR}"
echo "MNL lib config host: ${CONFIG_HOST}"

cd "${LIBMNL_SOURCE_DIR}"
autoreconf -f -i
./configure --prefix=${LIBMNL_INSTALL_DIR} --host=${CONFIG_HOST}
make
make install
make clean
