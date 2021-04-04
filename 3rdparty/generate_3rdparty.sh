#!/bin/bash 
SCRIPT_FULL_NAME=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_FULL_NAME")

echo "==================== libssh ===================="
cd "${BASEDIR}/libssh-0.9.5"
mkdir -p build
cd build
cmake -DUNIT_TESTING=OFF -DCMAKE_INSTALL_PREFIX="${BASEDIR}/libssh-0.9.5/bin" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF ..
make && make install

echo "==================== libreadline ===================="
cd "${BASEDIR}/readline-8.1"
mkdir -p bin
./configure --prefix="${BASEDIR}/readline-8.1/bin"
make && make install

echo "==================== lua ===================="
cd "${BASEDIR}/lua-5.4.2"
make && make local

