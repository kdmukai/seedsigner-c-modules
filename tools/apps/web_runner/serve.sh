#!/usr/bin/env bash
# Serve the built web_runner bundle to the local browser AND external clients on
# the LAN and the tailnet (binds 0.0.0.0), AND watch tools/scenarios/scenarios.json,
# auto-regenerating the localized catalogs + re-staging the served assets on every
# save.
#
# Because fonts + scenarios are fetched at runtime, the dev loop is just:
#   edit tools/scenarios/scenarios.json  ->  hard-refresh the browser
# No WASM rebuild. (Run build.sh once first to produce the bundle; rebuild only
# when C++ / shell.html change. Font pack changes still need build_fontpacks.py.)
#
# Usage:
#   bash tools/apps/web_runner/serve.sh           # port 8000
#   bash tools/apps/web_runner/serve.sh 9000       # custom port
#
# If a firewall is active, allow the port (e.g. `sudo ufw allow <port>/tcp`).
# Tailnet access also requires the device to be on your tailnet.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Repo root is three levels up: tools/apps/web_runner -> tools/apps -> tools -> repo.
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
PORT="${1:-8000}"
# The emscripten build output (the bundle: index.html web runner + index.js/.wasm).
BUNDLE_DIR="${SCRIPT_DIR}/build-wasm"
# The assembled site we actually serve — the SAME layout GitHub Pages publishes (the web
# gallery at the root /, the web runner at /runner/), so local dev and Pages are structurally
# identical and relative links (e.g. the gallery's runner/ jump) behave the same in both.
SITE_DIR="${SCRIPT_DIR}/site"
SERVE_DIR="${SITE_DIR}"
SCENARIOS="${REPO_ROOT}/tools/scenarios/scenarios.json"

if [ ! -f "${BUNDLE_DIR}/index.html" ]; then
  echo "No build found at ${BUNDLE_DIR}/index.html — run 'bash tools/apps/web_runner/build.sh' first." >&2
  exit 1
fi

# Regenerate the localized scenario catalogs from scenarios.json and re-stage the
# served assets into BOTH app dirs (the gallery at the root + /runner/ each carry their own
# assets/, like the deployed site). Runtime-fetched, so the browser just needs a hard-refresh.
regen() {
  if python3 "${REPO_ROOT}/tools/i18n/gen_localized_scenarios.py" >/dev/null 2>&1 \
     && python3 "${SCRIPT_DIR}/stage_assets.py" --dest "${SITE_DIR}"        >/dev/null 2>&1 \
     && python3 "${SCRIPT_DIR}/stage_assets.py" --dest "${SITE_DIR}/runner" >/dev/null 2>&1; then
    echo "[$(date +%H:%M:%S)] regenerated localized catalogs + re-staged — hard-refresh the browser"
  else
    echo "[$(date +%H:%M:%S)] regenerate FAILED — check tools/scenarios/scenarios.json (valid JSON?)" >&2
  fi
}

# Assemble the served site from the freshly-built bundle (mirrors CI's assemble-site).
# Re-copy the gallery shell into the bundle first so a gallery.html source edit is picked
# up on a serve restart without a full WASM rebuild. assemble-site rm's + rebuilds
# SITE_DIR, so run it ONCE here; regen only re-stages assets (no rebuild) thereafter.
cp "${SCRIPT_DIR}/gallery.html" "${BUNDLE_DIR}/"
( cd "$REPO_ROOT" && scripts/ci/ci.sh assemble-site "$SITE_DIR" >/dev/null )

# Stage once so the served assets match the current scenarios.json.
regen

# Pick a likely LAN IPv4 (skip loopback, docker bridges, and tailnet 100.64/10).
lan_ip="$(hostname -I 2>/dev/null | tr ' ' '\n' \
  | grep -E '^(192\.168|10\.|172\.(1[6-9]|2[0-9]|3[01]))\.' \
  | grep -vE '^172\.1[78]\.' | head -1 || true)"

echo "Serving ${SITE_DIR} on 0.0.0.0:${PORT}  (gallery / · runner /runner/)"
echo "  Local:   http://localhost:${PORT}/"
[ -n "$lan_ip" ] && echo "  LAN:     http://${lan_ip}:${PORT}/"

# Tailnet URLs (MagicDNS name preferred, IP as fallback).
if command -v tailscale >/dev/null 2>&1; then
  ts_name="$(tailscale status --self --json 2>/dev/null \
    | python3 -c "import sys,json; print(json.load(sys.stdin).get('Self',{}).get('DNSName','').rstrip('.'))" 2>/dev/null || true)"
  ts_ip="$(tailscale ip -4 2>/dev/null | head -1 || true)"
  [ -n "$ts_name" ] && echo "  Tailnet: http://${ts_name}:${PORT}/"
  [ -n "$ts_ip" ]   && echo "  Tailnet: http://${ts_ip}:${PORT}/"
fi

# Serve in the background so we can watch alongside; clean it up on exit.
# Use the no-cache server (not `python3 -m http.server`) so a re-stage or rebuild
# is picked up on a normal refresh — plain http.server lets the browser cache the
# runtime-fetched locales.json / .wasm and hide newly added locales/packs.
python3 "${SCRIPT_DIR}/nocache_server.py" "${PORT}" --bind 0.0.0.0 --directory "${SERVE_DIR}" &
SERVE_PID=$!
trap 'kill "$SERVE_PID" 2>/dev/null || true; echo; echo "stopped server + watcher"' INT TERM

echo "Watching tools/scenarios/scenarios.json (Ctrl-C to stop server + watcher)…"

# Event-driven via inotify when available; otherwise poll the file mtime.
if command -v inotifywait >/dev/null 2>&1; then
  # close_write covers editor saves; move_self/modify cover atomic-rename saves.
  while inotifywait -q -e close_write,modify,move_self "$SCENARIOS" >/dev/null 2>&1; do
    regen
  done
else
  last="$(stat -c %Y "$SCENARIOS" 2>/dev/null || echo 0)"
  while :; do
    sleep 1
    cur="$(stat -c %Y "$SCENARIOS" 2>/dev/null || echo 0)"
    if [ "$cur" != "$last" ]; then last="$cur"; regen; fi
  done
fi
