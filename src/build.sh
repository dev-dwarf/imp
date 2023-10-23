#!/bin/bash
pushd "$(dirname "$0")"

mkdir -p ../build
cc ../src/template_raylib.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -g -o ../build/template_raylib

popd
