#!/bin/bash
set -e

echo "Installing dependencies"
su root -c "apt-get update"
su root -c "apt-get install -y --no-install-recommends \
    openjdk-8-jre \
    openjdk-8-jdk \
    python3 \
    python3-dev \
    python3-pip \
    python3-setuptools \
    python3-wheel \
    tcl"

echo "Building the tester"
cd ../tester
make
cd ..

echo "Finished setup"