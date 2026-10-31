#!/bin/bash
# Needs bash: emsdk_env.sh cannot find its own folder in a plain sh like dash.
# Builds the browser version and serves it on this machine only.
# Usage: ./serve.sh [port]
# To pick up changes, run this again or just `cmake --build build-web`, then reload the page.
set -e

cd "$(dirname "$0")"
port="${1:-8000}"
emsdk="${EMSDK_DIR:-$HOME/Dokumente/Programming/dependencies/emsdk}"

source "$emsdk/emsdk_env.sh" > /dev/null 2>&1

if ! command -v emcmake > /dev/null; then
    echo "emcmake not found: no emsdk in $emsdk? Set EMSDK_DIR to its folder." >&2
    exit 1
fi

emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build build-web -j "$(nproc)"

echo "Serving on http://127.0.0.1:$port (Ctrl+C stops it)"
exec python3 -m http.server "$port" --bind 127.0.0.1 --directory build-web/site
