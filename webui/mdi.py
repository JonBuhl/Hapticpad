#!/usr/bin/env python3
"""Material Design Icons als Quelle fuer die BMP-Erzeugung.

MDI liegt als Webfont vor: jedes Icon ist ein Glyph in der privaten Unicode-
Zone. Statt SVGs zu rastern (braucht Cairo) rendern wir das Zeichen mit Pillow —
fuer ein 15x15 1-Bit-Icon ist das genau das Richtige.

Gebraucht werden zwei Dateien in webui/assets/:
  materialdesignicons-webfont.ttf   die Schrift
  mdi-meta.json                     Name -> Codepoint (meta.json aus @mdi/svg)

Beide koennen von Hand hineinkopiert oder ueber install_from_cdn() geholt
werden. Ohne meta.json laesst sich immer noch direkt ein Codepoint eingeben.
"""

import json
import os

ASSET_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "assets")
FONT_PATH = os.path.join(ASSET_DIR, "materialdesignicons-webfont.ttf")
META_PATH = os.path.join(ASSET_DIR, "mdi-meta.json")

MDI_VERSION = "7.4.47"
FONT_URL = ("https://cdn.jsdelivr.net/npm/@mdi/font@%s/fonts/"
            "materialdesignicons-webfont.ttf" % MDI_VERSION)
META_URL = "https://cdn.jsdelivr.net/npm/@mdi/svg@%s/meta.json" % MDI_VERSION

_icons_cache = None


def font_path():
    """Pfad zur MDI-Schrift, oder None. Nimmt auch eine abweichend benannte TTF."""
    if os.path.isfile(FONT_PATH):
        return FONT_PATH
    if os.path.isdir(ASSET_DIR):
        for entry in sorted(os.listdir(ASSET_DIR)):
            low = entry.lower()
            if low.endswith(".ttf") and "materialdesignicons" in low:
                return os.path.join(ASSET_DIR, entry)
    return None


def load_icons():
    """[{name, codepoint, aliases, tags}] aus meta.json, sonst leere Liste."""
    global _icons_cache
    if _icons_cache is not None:
        return _icons_cache

    if not os.path.isfile(META_PATH):
        _icons_cache = []
        return _icons_cache

    try:
        with open(META_PATH, "r", encoding="utf-8") as handle:
            raw = json.load(handle)
    except (OSError, ValueError):
        _icons_cache = []
        return _icons_cache

    icons = []
    for entry in raw:
        name = entry.get("name")
        codepoint = entry.get("codepoint")
        if not name or not codepoint:
            continue
        icons.append(dict(
            name=name,
            codepoint=str(codepoint).lower(),
            aliases=entry.get("aliases") or [],
            tags=entry.get("tags") or [],
        ))
    icons.sort(key=lambda i: i["name"])
    _icons_cache = icons
    return icons


def status():
    icons = load_icons()
    path = font_path()
    return dict(
        fontAvailable=bool(path),
        fontPath=path or FONT_PATH,
        metaAvailable=bool(icons),
        count=len(icons),
        version=MDI_VERSION,
        assetDir=ASSET_DIR,
    )


def search(query, limit=200):
    """Namen, Aliase und Tags durchsuchen. Leere Anfrage = Anfang der Liste."""
    icons = load_icons()
    query = (query or "").strip().lower()
    if not query:
        return icons[:limit]

    exact, prefix, other = [], [], []
    for icon in icons:
        name = icon["name"]
        if name == query:
            exact.append(icon)
        elif name.startswith(query):
            prefix.append(icon)
        elif query in name or any(query in a.lower() for a in icon["aliases"]) \
                or any(query in t.lower() for t in icon["tags"]):
            other.append(icon)
        if len(exact) + len(prefix) + len(other) >= limit * 3:
            break

    return (exact + prefix + other)[:limit]


def codepoint_of(name):
    for icon in load_icons():
        if icon["name"] == name:
            return int(icon["codepoint"], 16)
    return None


def install_from_cdn(timeout=60):
    """Font und meta.json von jsDelivr holen. Wird nur auf Klick ausgeloest."""
    import urllib.request

    os.makedirs(ASSET_DIR, exist_ok=True)
    written = []

    for url, target in ((FONT_URL, FONT_PATH), (META_URL, META_PATH)):
        request = urllib.request.Request(url, headers={"User-Agent": "HapticPad-WebUI"})
        with urllib.request.urlopen(request, timeout=timeout) as response:
            data = response.read()
        if len(data) < 1024:
            raise RuntimeError("Antwort von %s war unerwartet klein (%d Byte)" % (url, len(data)))
        with open(target, "wb") as handle:
            handle.write(data)
        written.append(dict(file=os.path.basename(target), bytes=len(data)))

    global _icons_cache
    _icons_cache = None
    return written
