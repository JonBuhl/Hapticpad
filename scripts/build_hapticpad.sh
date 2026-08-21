#!/usr/bin/env bash
# build_hapticpad.sh — Baut die Hapticpad-Firmware und archiviert das UF2
# AUTOMATISCH mit Beschreibung in firmware/ (untracked).
#
# Usage:
#   scripts/build_hapticpad.sh "Beschreibung der Änderungen" [version-tag]
#   scripts/build_hapticpad.sh --no-archive   # nur bauen, nichts archivieren
#
# Ohne Beschreibung wird die letzte Commit-Message als Beschreibung genommen.
# Ohne version-tag wird der kurze Commit-Hash als Tag genommen.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FQBN="${FQBN:-rp2040:rp2040:waveshare_rp2040_plus:flash=16777216_0,usbstack=tinyusb}"
SKETCH="$REPO_ROOT/Software/MacroPad"

if [ "${1:-}" = "--no-archive" ]; then
  echo "=== Build ohne Archivierung ==="
  arduino-cli compile --fqbn "$FQBN" "$SKETCH"
  exit 0
fi

DESC="${1:-}"
TAG="${2:-}"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT

echo "=== Build: $SKETCH ==="
arduino-cli compile --fqbn "$FQBN" --build-path "$BUILD_DIR" "$SKETCH"

UF2="$BUILD_DIR/MacroPad.ino.uf2"
[ -f "$UF2" ] || { echo "Fehler: Build hat kein UF2 erzeugt" >&2; exit 1; }

if [ -z "$DESC" ]; then
  DESC="$(git -C "$REPO_ROOT" log -1 --pretty=%s 2>/dev/null || echo 'Build ohne Commit-Info')"
fi
if [ -z "$TAG" ]; then
  TAG="$(git -C "$REPO_ROOT" rev-parse --short HEAD 2>/dev/null || echo 'unversioned')"
fi

"$REPO_ROOT/scripts/save_uf2.sh" "$UF2" "$TAG" "$DESC"
