#!/usr/bin/env bash
# save_uf2.sh — Archiviert einen gebauten UF2 MIT Pflicht-Beschreibung in
# firmware/ (bewusst untracked, siehe .gitignore).
#
# Usage:
#   scripts/save_uf2.sh <uf2-datei> <version-tag> <beschreibung...>
#
# Beispiele:
#   scripts/save_uf2.sh build/MacroPad.ino.uf2 v1.2-friction-fix \
#       "Friction-Modus: Stärke-Ramping korrigiert"
#
# Erzeugt: firmware/<datum>_<version-tag>/MacroPad_<version-tag>.uf2 + notes.md
# Beschreibung ist PFLICHT — ohne Argument bricht das Skript ab.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIRMWARE_DIR="$REPO_ROOT/firmware"

if [ "$#" -lt 3 ]; then
  echo "Usage: $0 <uf2-datei> <version-tag> <beschreibung...>" >&2
  echo "Beschreibung ist Pflicht — jede Version wird mit Code-Stand dokumentiert." >&2
  exit 1
fi

UF2_SRC="$1"
TAG="$2"
shift 2
DESC="$*"

# Validierung
[ -f "$UF2_SRC" ] || { echo "Fehler: UF2 nicht gefunden: $UF2_SRC" >&2; exit 1; }
[[ "$UF2_SRC" == *.uf2 ]] || { echo "Fehler: keine .uf2-Datei: $UF2_SRC" >&2; exit 1; }
[ -n "$DESC" ] || { echo "Fehler: Beschreibung fehlt (Pflicht!)" >&2; exit 1; }

# Tag normalisieren: nur [A-Za-z0-9._-] erlaubt, Rest -> '-'
TAG_CLEAN="$(printf '%s' "$TAG" | tr -c 'A-Za-z0-9._-' '-')"
BUILD_DATE="$(date +%Y-%m-%d)"
DIR="$FIRMWARE_DIR/${BUILD_DATE}_${TAG_CLEAN}"
mkdir -p "$DIR"

UF2_DEST="$DIR/MacroPad_${TAG_CLEAN}.uf2"
cp "$UF2_SRC" "$UF2_DEST"

# Git-Info
GIT_BRANCH="$(git -C "$REPO_ROOT" branch --show-current 2>/dev/null || echo '(kein git)')"
GIT_SHORT="$(git -C "$REPO_ROOT" rev-parse --short HEAD 2>/dev/null || echo '?')"
GIT_FULL="$(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || echo '?')"
GIT_MSG="$(git -C "$REPO_ROOT" log -1 --pretty=%s 2>/dev/null || echo '?')"
GIT_DIRTY="$(git -C "$REPO_ROOT" status --porcelain 2>/dev/null | grep -v '^??' | head -5 || true)"

SHA="$(sha256sum "$UF2_DEST" | cut -d' ' -f1)"
SIZE="$(stat -c%s "$UF2_DEST")"
FQBN="${FQBN:-rp2040:rp2040:waveshare_rp2040_plus:flash=16777216_0,usbstack=tinyusb}"

# notes.md schreiben
cat > "$DIR/notes.md" <<EOF
# MacroPad ${TAG_CLEAN} — ${BUILD_DATE}

## Beschreibung / Zustand des Codes
${DESC}

## Git-Stand
- Branch: ${GIT_BRANCH}
- Commit: ${GIT_SHORT} (${GIT_FULL})
- Commit-Message: ${GIT_MSG}
- Working tree: ${GIT_DIRTY:-sauber}

## Build
- FQBN: ${FQBN}
- Core: rp2040 5.5.0
- Größe: ${SIZE} Bytes
- SHA256: ${SHA}

## SD-Karte
(SD-Stand hier eintragen, falls relevant — z.B. config.xml-Version, Icons)

## Flashen
1. BOOTSEL gedrückt halten + USB einstecken → Laufwerk RPI-RP2 erscheint
2. MacroPad_${TAG_CLEAN}.uf2 auf RPI-RP2 kopieren
3. Board neu einstecken → Firmware startet
EOF

echo "✅ Archiviert: $DIR"
echo "   UF2:    $UF2_DEST ($SIZE Bytes)"
echo "   SHA256: $SHA"
echo "   notes:  $DIR/notes.md  ← Beschreibung bei Bedarf ergänzen"
