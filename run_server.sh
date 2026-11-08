#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
PORT="${1:-8080}"

cd "$ROOT_DIR"

echo "Building the macOS/POSIX web backend..."
make server

echo "Starting Optimal Delivery Route Planning at http://localhost:${PORT}"
if [ -d "/Applications/Google Chrome.app" ]; then
  open -a "Google Chrome" "http://localhost:${PORT}"
elif command -v open >/dev/null 2>&1; then
  open "http://localhost:${PORT}"
fi

exec "./bin/delivery_server" "$PORT"
