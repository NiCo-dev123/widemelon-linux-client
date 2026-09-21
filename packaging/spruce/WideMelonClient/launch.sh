#!/bin/sh

APP_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)" || exit 1
cd "$APP_DIR" || exit 1

# An optional lib/ directory can contain the SDL2 runtime used by the ARM64 build.
export LD_LIBRARY_PATH="$APP_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

exec "$APP_DIR/widemelon-client" > "$APP_DIR/widemelon-client.log" 2>&1
