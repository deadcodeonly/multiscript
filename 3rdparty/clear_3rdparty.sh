#!/bin/bash 
SCRIPT_FULL_NAME=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_FULL_NAME")

echo "==================== libssh ===================="
cd "${BASEDIR}/libssh-0.9.5" && rm -rf build bin

echo "==================== libreadline ===================="
cd "${BASEDIR}/readline-8.1" && make clean && rm -rf bin

echo "==================== lua ===================="
cd "${BASEDIR}/lua-5.4.2" && make clean && rm -rf install

