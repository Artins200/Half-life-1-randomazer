#!/bin/sh
# Copy only the installed Opposing Force client into this separate mod.
# Never overwrite an existing client or touch valve, gearbox, or SAVE.
set -eu
MOD=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
SOURCE="$MOD/../gearbox/cl_dlls/client.so"
TARGET_DIR="$MOD/cl_dlls"
TARGET="$TARGET_DIR/client.so"
if [ ! -f "$MOD/../gearbox/dlls/opfor.so" ] && [ ! -f "$MOD/../gearbox/dlls/opfor_i386.so" ]; then
    echo 'ERROR: gearbox/dlls/opfor.so not found. Install Opposing Force in the same Half-Life folder.' >&2
    exit 1
fi
if [ ! -f "$SOURCE" ]; then
    echo 'ERROR: gearbox/cl_dlls/client.so not found. Verify the Opposing Force installation.' >&2
    exit 1
fi
if [ ! -f "$MOD/dlls/hl.so" ]; then
    echo "ERROR: this mod's dlls/hl.so is missing." >&2
    exit 1
fi
if [ -L "$TARGET_DIR" ] || [ -L "$TARGET" ]; then
    echo 'ERROR: cl_dlls or client.so is a symlink; install the client manually.' >&2
    exit 1
fi
if [ -e "$TARGET" ]; then
    if cmp -s "$SOURCE" "$TARGET"; then
        echo 'Opposing Force client is already installed and matches.'
        exit 0
    fi
    echo 'ERROR: cl_dlls/client.so already exists and differs. Back it up and replace it manually if desired.' >&2
    exit 1
fi
mkdir -p "$TARGET_DIR"
# Refuse to overwrite even if another process creates the destination meanwhile.
cp -n "$SOURCE" "$TARGET"
if ! cmp -s "$SOURCE" "$TARGET"; then
    echo 'ERROR: copied client does not match. Check the files manually.' >&2
    exit 1
fi
echo 'Opposing Force client copied. Launch Half-Life with -game hl_randomizer_opfor.'
