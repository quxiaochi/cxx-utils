#! /usr/bin/env bash

rm -rf cmake-build
mkdir -p cmake-build
cd cmake-build;cmake ..; make -j 4
