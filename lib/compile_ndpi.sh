#!/bin/bash

set -e

LIBNDPI_SOURCE_DIR=./nDPI
LIBNDPI_INSTALL_DIR=$1/ndpi
LIBPCAP_INSTALL_DIR=$1/pcap
LIBPCAP_INCLUDE_PATH=${LIBPCAP_INSTALL_DIR}/include
LIBPCAP_LIB_PATH=${LIBPCAP_INSTALL_DIR}/lib

CONFIG_HOST=$3

echo "NDPI lib source dir: ${LIBNDPI_SOURCE_DIR}"
echo "NDPI lib install dir: ${LIBNDPI_INSTALL_DIR}"
echo "NDPI lib config host: ${CONFIG_HOST}"

rm -rf "${LIBNDPI_SOURCE_DIR}"
git clone --depth 1 --branch 3.4 https://github.com/ntop/nDPI

cd "${LIBNDPI_SOURCE_DIR}"
./autogen.sh

CFLAGS="-I$LIBPCAP_INCLUDE_PATH" LDFLAGS="-L$LIBPCAP_LIB_PATH" ./configure --prefix=${LIBNDPI_INSTALL_DIR} --host=${CONFIG_HOST}
make
make install
make clean
cd ../
rm -rf "${LIBNDPI_SOURCE_DIR}"
