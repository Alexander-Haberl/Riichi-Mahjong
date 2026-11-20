#!/bin/bash
# Builds the browser version and serves it on this machine only.
# Usage: ./serve.sh [port]
# Needs Emscripten: either emcmake on the PATH (e.g. after sourcing emsdk_env.sh),
# or EMSDK_DIR set to the emsdk folder, whose environment this script then loads.
# To pick up changes, run this again or just `cmake --build build-web`, then reload the page.
# Needs bash: emsdk_env.sh cannot find its own folder in a plain sh like dash.
set -e

cd "$(dirname "$0")"
port="${1:-8000}"

if ! command -v emcmake > /dev/null && [ -n "${EMSDK_DIR:-}" ]; then
    if [ ! -f "$EMSDK_DIR/emsdk_env.sh" ]; then
        echo "EMSDK_DIR is set to $EMSDK_DIR, but there is no emsdk_env.sh in it." >&2
        exit 1
    fi
    source "$EMSDK_DIR/emsdk_env.sh" > /dev/null 2>&1
fi

if ! command -v emcmake > /dev/null; then
    echo "Emscripten not found. Either put emcmake on the PATH (source emsdk_env.sh)," >&2
    echo "or set EMSDK_DIR to your emsdk folder, e.g.:" >&2
    echo "    EMSDK_DIR=~/emsdk ./serve.sh" >&2
    exit 1
fi

emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build build-web -j "$(nproc)"

echo "Serving on http://127.0.0.1:$port (Ctrl+C stops it)"
# Python's http.server, but telling the browser not to cache: otherwise it may
# keep using an old engine.wasm next to newer scripts after a rebuild.
exec python3 -c '
import functools, http.server, sys

class NoCacheHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

handler = functools.partial(NoCacheHandler, directory=sys.argv[2])
http.server.ThreadingHTTPServer(("127.0.0.1", int(sys.argv[1])), handler).serve_forever()
' "$port" build-web/site
