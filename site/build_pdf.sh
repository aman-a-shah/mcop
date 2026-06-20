#!/usr/bin/env bash
# Render the showcase to a PDF.
#
# The HTML/CSS/JS in this folder is the rendering engine; Chrome's headless
# print pipeline turns it into the deliverable. We serve over HTTP (the page
# fetch()es data.json, blocked under file://), let JS finish building the SVG
# charts and typesetting the math, then print to PDF.
#
# Usage:  bash site/build_pdf.sh
# Output: site/monte-carlo-options-pricer.pdf
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SITE="$ROOT/site"
OUT="$SITE/monte-carlo-options-pricer.pdf"
PORT="${PORT:-8017}"

# Locate Chrome / Chromium.
CHROME=""
for c in \
  "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
  "/Applications/Chromium.app/Contents/MacOS/Chromium" \
  "$(command -v google-chrome 2>/dev/null || true)" \
  "$(command -v chromium 2>/dev/null || true)"; do
  if [ -n "$c" ] && [ -x "$c" ]; then CHROME="$c"; break; fi
done
if [ -z "$CHROME" ]; then echo "error: Chrome/Chromium not found" >&2; exit 1; fi

# 1. Regenerate data from a live engine run (skip with REGEN=0).
if [ "${REGEN:-1}" = "1" ]; then
  echo "→ exporting data.json"
  python3 "$SITE/export_data.py"
fi

# 2. Serve the folder.
python3 -m http.server "$PORT" --directory "$SITE" >/dev/null 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null || true' EXIT
sleep 1

# 3. Print to PDF. virtual-time-budget lets the async fetch + KaTeX + SVG
#    rendering settle before capture; the window width makes the on-screen
#    chart render match the print content width.
echo "→ printing PDF"
"$CHROME" --headless --disable-gpu --no-pdf-header-footer \
  --virtual-time-budget=12000 --run-all-compositor-stages-before-draw \
  --window-size=760,1100 \
  --print-to-pdf="$OUT" \
  "http://localhost:$PORT/index.html" >/dev/null 2>&1

echo "✓ wrote $OUT ($(du -h "$OUT" | cut -f1))"
