#!/bin/bash
# Builds bin/capsim. Needs gcc, wayland (wayland-scanner), cairo and pkg-config,
# all present on a stock Omarchy install. Nothing is downloaded.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p bin build
wayland-scanner client-header protocol/input-method-unstable-v2.xml build/input-method-unstable-v2-client-protocol.h
wayland-scanner private-code  protocol/input-method-unstable-v2.xml build/input-method-unstable-v2-protocol.c
gcc -O2 -Wall -Ibuild -o bin/capsim src/capsim.c build/input-method-unstable-v2-protocol.c \
  $(pkg-config --cflags --libs wayland-client cairo) -lm
echo "built bin/capsim"
