#!/usr/bin/env python3
"""Katalog der erzeugten BMP-Icons.

Liegt in webui/catalog/: pro Icon eine .bmp im Firmware-Format plus ein
index.json mit Name, Tags und Herkunft. Die Bilddatei ist die Wahrheit — geht
der Index verloren, werden die BMPs beim naechsten Start wieder eingesammelt.
"""

import json
import os
import re
import time

import bmpgen

CATALOG_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "catalog")
INDEX_PATH = os.path.join(CATALOG_DIR, "index.json")

SLOT_COUNT = 6


def _slug(text):
    slug = re.sub(r"[^a-zA-Z0-9_-]+", "-", (text or "icon").strip().lower()).strip("-")
    return slug or "icon"


def _read_index():
    if not os.path.isfile(INDEX_PATH):
        return {}
    try:
        with open(INDEX_PATH, "r", encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, ValueError):
        return {}
    return {entry["id"]: entry for entry in data if isinstance(entry, dict) and entry.get("id")}


def _write_index(entries):
    os.makedirs(CATALOG_DIR, exist_ok=True)
    ordered = sorted(entries.values(), key=lambda e: (-e.get("created", 0), e.get("name", "")))
    with open(INDEX_PATH, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(ordered, handle, indent=2, ensure_ascii=False)
        handle.write("\n")


def _sync():
    """Index und Ordnerinhalt abgleichen: verwaiste Eintraege raus, neue rein."""
    os.makedirs(CATALOG_DIR, exist_ok=True)
    entries = _read_index()

    on_disk = set()
    for entry in sorted(os.listdir(CATALOG_DIR)):
        if not entry.lower().endswith(".bmp"):
            continue
        icon_id = os.path.splitext(entry)[0]
        on_disk.add(icon_id)
        if icon_id not in entries:
            entries[icon_id] = dict(
                id=icon_id,
                name=icon_id.rsplit("-", 1)[0].replace("-", " ").strip() or icon_id,
                tags=[],
                source="datei",
                size=bmpgen.DEFAULT_SIZE,
                created=int(os.path.getmtime(os.path.join(CATALOG_DIR, entry))),
            )

    for icon_id in list(entries):
        if icon_id not in on_disk:
            del entries[icon_id]

    _write_index(entries)
    return entries


def path_of(icon_id):
    return os.path.join(CATALOG_DIR, "%s.bmp" % icon_id)


def list_entries():
    entries = _sync()
    out = []
    for entry in sorted(entries.values(), key=lambda e: (-e.get("created", 0), e.get("name", ""))):
        try:
            with open(path_of(entry["id"]), "rb") as handle:
                pixels = bmpgen.bmp_to_pixels(handle.read(), entry.get("size", bmpgen.DEFAULT_SIZE))
        except (OSError, ValueError):
            continue
        item = dict(entry)
        item["pixels"] = pixels
        out.append(item)
    return out


def get_pixels(icon_id):
    entries = _sync()
    entry = entries.get(icon_id)
    if not entry:
        raise KeyError(icon_id)
    with open(path_of(icon_id), "rb") as handle:
        return bmpgen.bmp_to_pixels(handle.read(), entry.get("size", bmpgen.DEFAULT_SIZE))


def save(pixels, name, tags=None, source="webui", size=None):
    size = size or len(pixels)
    pixels = bmpgen.normalise_pixels(pixels, size)

    entries = _sync()
    icon_id = "%s-%s" % (_slug(name), format(int(time.time() * 1000) % 0xFFFFFF, "06x"))

    os.makedirs(CATALOG_DIR, exist_ok=True)
    with open(path_of(icon_id), "wb") as handle:
        handle.write(bmpgen.pixels_to_bmp(pixels))

    entry = dict(
        id=icon_id,
        name=(name or "Icon").strip(),
        tags=[t.strip() for t in (tags or []) if str(t).strip()],
        source=source,
        size=size,
        created=int(time.time()),
    )
    entries[icon_id] = entry
    _write_index(entries)

    result = dict(entry)
    result["pixels"] = pixels
    return result


def rename(icon_id, name=None, tags=None):
    entries = _sync()
    if icon_id not in entries:
        raise KeyError(icon_id)
    if name is not None:
        entries[icon_id]["name"] = str(name).strip() or entries[icon_id]["name"]
    if tags is not None:
        entries[icon_id]["tags"] = [str(t).strip() for t in tags if str(t).strip()]
    _write_index(entries)
    return entries[icon_id]


def delete(icon_id):
    entries = _sync()
    if icon_id not in entries:
        raise KeyError(icon_id)
    try:
        os.remove(path_of(icon_id))
    except OSError:
        pass
    del entries[icon_id]
    _write_index(entries)


# --- SD-Karte ---------------------------------------------------------------

def sd_profiles(sd_root):
    """Alle Icon-Ordner der SD-Karte mit ihren sechs Slots."""
    out = []
    if not os.path.isdir(sd_root):
        return out

    for entry in sorted(os.listdir(sd_root)):
        folder = os.path.join(sd_root, entry)
        if not os.path.isdir(folder):
            continue

        slots = []
        for slot in range(1, SLOT_COUNT + 1):
            icon_path = os.path.join(folder, "%d.bmp" % slot)
            pixels, problem = None, None
            if os.path.isfile(icon_path):
                try:
                    with open(icon_path, "rb") as handle:
                        data = handle.read()
                    pixels = bmpgen.bmp_to_pixels(data, bmpgen.DEFAULT_SIZE)
                    problem = _check_format(data)
                except (OSError, ValueError) as error:
                    problem = str(error)
            slots.append(dict(slot=slot, pixels=pixels, problem=problem))

        if any(s["pixels"] for s in slots):
            out.append(dict(name=entry, slots=slots))

    return out


def _check_format(data):
    """Meldet, was loadBMP15x15() an dieser Datei auszusetzen haette."""
    import struct
    if len(data) < 30 or data[:2] != b"BM":
        return "keine BMP-Datei"
    width = struct.unpack_from("<i", data, 18)[0]
    height = struct.unpack_from("<i", data, 22)[0]
    depth = struct.unpack_from("<H", data, 28)[0]
    if depth != 1:
        return "%d Bit statt 1 Bit — die Firmware laedt das Icon nicht" % depth
    if width != 15 or abs(height) != 15:
        return "%dx%d statt 15x15 — die Firmware laedt das Icon nicht" % (width, abs(height))
    return None


def assign(sd_root, profile_name, slot, pixels):
    """Icon in <SD>/<Profil>/<slot>.bmp schreiben."""
    slot = int(slot)
    if not 1 <= slot <= SLOT_COUNT:
        raise ValueError("Slot muss zwischen 1 und %d liegen" % SLOT_COUNT)

    profile_name = str(profile_name).strip()
    if not profile_name or os.path.sep in profile_name or profile_name in (".", ".."):
        raise ValueError("Ungueltiger Profilname: %r" % profile_name)
    if "/" in profile_name or "\\" in profile_name:
        raise ValueError("Profilname darf keine Pfadtrenner enthalten")

    # Die Firmware liest immer 15x15, egal was der Editor anzeigt.
    pixels = bmpgen.normalise_pixels(pixels, bmpgen.DEFAULT_SIZE)

    folder = os.path.join(sd_root, profile_name)
    os.makedirs(folder, exist_ok=True)
    target = os.path.join(folder, "%d.bmp" % slot)
    with open(target, "wb") as handle:
        handle.write(bmpgen.pixels_to_bmp(pixels))
    return target


def import_from_sd(sd_root):
    """Alle Icons der SD-Karte in den Katalog uebernehmen (Duplikate raus)."""
    known = set()
    for entry in list_entries():
        known.add("".join(entry["pixels"]))

    added = 0
    for profile in sd_profiles(sd_root):
        for slot in profile["slots"]:
            if not slot["pixels"]:
                continue
            key = "".join(slot["pixels"])
            if key in known or key.count("1") == 0:
                continue
            known.add(key)
            save(slot["pixels"], "%s %d" % (profile["name"], slot["slot"]),
                 tags=[profile["name"], "sd"], source="sd-karte")
            added += 1
    return added
