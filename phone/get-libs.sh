#!/usr/bin/env bash
# Run ONCE on a computer with internet, from inside the phone/ folder:
#   bash get-libs.sh
# Downloads the map libraries into lib/ so the page never needs the internet.
set -euo pipefail
mkdir -p lib
get() { curl -fL --retry 3 -o "$2" "$1"; [ -s "$2" ] || { echo "FAILED: $2"; exit 1; }; echo "ok  $2"; }
get https://cdn.jsdelivr.net/npm/maplibre-gl@4/dist/maplibre-gl.js  lib/maplibre-gl.js
get https://cdn.jsdelivr.net/npm/maplibre-gl@4/dist/maplibre-gl.css lib/maplibre-gl.css
get https://cdn.jsdelivr.net/npm/pmtiles@3/dist/pmtiles.js          lib/pmtiles.js
echo "Done. Now host the phone/ folder over HTTPS (see README)."
