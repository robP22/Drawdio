#!/usr/bin/env bash

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

PLUGIN_NAME="Drawdio.vst3"
BUILD_DIR="./build"
PLUGIN_DIR="$HOME/Library/Audio/Plug-Ins/VST3"

SOURCE_PLUGIN="$BUILD_DIR/Drawdio_artefacts/VST3/$PLUGIN_NAME"
DEST_PLUGIN="$PLUGIN_DIR/$PLUGIN_NAME"

if [[ "${1:-}" == "--reconfigure" ]]; then
    echo "Reconfiguring build directory..."
    rm -rf "$BUILD_DIR"
    cmake -B "$BUILD_DIR"
fi

echo "Building..."
cmake --build "$BUILD_DIR" --target Drawdio_VST3 -j"$(sysctl -n hw.ncpu)"

echo "Installing..."
mkdir -p "$PLUGIN_DIR"
rm -rf "$DEST_PLUGIN"
cp -R "$SOURCE_PLUGIN" "$PLUGIN_DIR"

echo "Done."
