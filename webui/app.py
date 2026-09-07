#!/usr/bin/env python3
"""HapticPad Studio — WebUI fuer config.xml und die Icon-BMPs.

Start:
    python webui/app.py                 # oeffnet http://127.0.0.1:8765
    python webui/app.py --port 9000 --no-browser
    python webui/app.py --sd F:/        # Icons direkt auf die SD-Karte schreiben

Bewusst ohne Flask: nur Standardbibliothek plus Pillow, das fuer die Icons
ohnehin gebraucht wird. Der Server bindet auf 127.0.0.1 und ist damit nur
lokal erreichbar.
"""

import argparse
import base64
import json
import mimetypes
import os
import sys
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, HERE)

import bmpgen      # noqa: E402
import catalog     # noqa: E402
import configio    # noqa: E402
import mdi         # noqa: E402
import schema      # noqa: E402

STATIC_DIR = os.path.join(HERE, "static")

# Ziele beim Speichern. Die Firmware liest die XML von der SD-Karte, das Repo
# haelt zwei byte-identische Kopien.
CONFIG_TARGETS = {
    "repo": os.path.join(REPO, "config.xml"),
    "sdcard": os.path.join(REPO, "Example SD Card", "config.xml"),
}
YAML_TARGET = os.path.join(REPO, "config.yaml")

STATE = dict(sd_root=os.path.join(REPO, "Example SD Card"))


# --- API --------------------------------------------------------------------

def api_meta():
    return dict(
        settings=schema.SETTINGS,
        settingsOrder=schema.SETTINGS_ORDER,
        wheelModes=schema.WHEEL_MODES,
        wheelModeHelp=schema.WHEEL_MODE_HELP,
        ledModes=schema.LED_MODES,
        ledModeHelp=schema.LED_MODE_HELP,
        keycodes=[dict(code=c, name=n, group=g) for c, n, g in schema.KEYCODES],
        shapes=bmpgen.list_shapes(),
        fonts=bmpgen.list_fonts()[:400],
        mdi=mdi.status(),
        sizes=list(bmpgen.SIZES),
        defaultSize=bmpgen.DEFAULT_SIZE,
        buttonsPerProfile=schema.BUTTONS_PER_PROFILE,
        actionsPerButton=schema.ACTIONS_PER_BUTTON,
        paths=dict(repo=REPO, sd=STATE["sd_root"],
                   configXml=CONFIG_TARGETS["repo"],
                   configSd=CONFIG_TARGETS["sdcard"],
                   configYaml=YAML_TARGET,
                   catalog=catalog.CATALOG_DIR),
        defaults=dict(settings=schema.default_settings(),
                      profile=schema.default_profile(),
                      button=schema.default_button()),
    )


def api_config_load(which="repo"):
    path = CONFIG_TARGETS.get(which, CONFIG_TARGETS["repo"])
    if not os.path.isfile(path):
        return dict(config=dict(Settings=schema.default_settings(), Profiles=[]),
                    source=path, existed=False)
    return dict(config=configio.load_xml_file(path), source=path, existed=True)


def api_config_preview(body):
    config = body.get("config") or {}
    return dict(xml=configio.build_xml(config),
                yaml=configio.build_yaml(config),
                notes=configio.lint(config))


def api_config_save(body):
    config = body.get("config") or {}
    targets = body.get("targets") or ["repo", "sdcard"]
    xml = configio.build_xml(config)

    written = []
    for key in targets:
        if key == "yaml":
            configio.write_text(YAML_TARGET, configio.build_yaml(config))
            written.append(YAML_TARGET)
        elif key in CONFIG_TARGETS:
            configio.write_text(CONFIG_TARGETS[key], xml)
            written.append(CONFIG_TARGETS[key])
        elif key == "sdroot":
            target = os.path.join(STATE["sd_root"], "config.xml")
            configio.write_text(target, xml)
            written.append(target)

    return dict(written=written, notes=configio.lint(config))


def api_config_import(body):
    text = body.get("text") or ""
    if not text.strip():
        raise ValueError("Keine Daten uebergeben")
    return dict(config=configio.parse_xml(text))


def _decode_data_url(value):
    if not value:
        raise ValueError("Kein Bild uebergeben")
    if "," in value and value.strip().startswith("data:"):
        value = value.split(",", 1)[1]
    return base64.b64decode(value)


def api_bmp_render(body):
    source = body.get("source") or "text"
    size = int(body.get("size") or bmpgen.DEFAULT_SIZE)
    if size not in bmpgen.SIZES:
        size = bmpgen.DEFAULT_SIZE

    if source == "text":
        pixels = bmpgen.render_text(
            body.get("text", ""), size=size,
            font_path=body.get("fontPath") or None,
            font_size=int(body.get("fontSize") or 0),
            bold=bool(body.get("bold")),
            offset_x=int(body.get("offsetX") or 0),
            offset_y=int(body.get("offsetY") or 0),
            threshold=int(body.get("threshold") or 110),
        )
    elif source == "image":
        pixels = bmpgen.render_image(
            _decode_data_url(body.get("data")), size=size,
            threshold=int(body.get("threshold") or 128),
            invert=bool(body.get("invert")),
            dither=bool(body.get("dither")),
            fit=body.get("fit") or "contain",
            offset_x=int(body.get("offsetX") or 0),
            offset_y=int(body.get("offsetY") or 0),
            trim=bool(body.get("trim", True)),
            gamma=float(body.get("gamma") or 1.0),
        )
    elif source == "mdi":
        path = mdi.font_path()
        if not path:
            raise ValueError("MDI-Schrift fehlt. Im Reiter BMP-Generator installieren "
                             "oder materialdesignicons-webfont.ttf nach webui/assets/ legen.")
        name = body.get("icon")
        codepoint = body.get("codepoint")
        if name and not codepoint:
            codepoint = mdi.codepoint_of(name)
            if codepoint is None:
                raise ValueError("Unbekanntes MDI-Icon: %s" % name)
        elif codepoint:
            codepoint = int(str(codepoint), 16) if isinstance(codepoint, str) else int(codepoint)
        else:
            raise ValueError("Weder Icon-Name noch Codepoint uebergeben")
        pixels = bmpgen.render_glyph(
            path, codepoint, size=size,
            font_size=int(body.get("fontSize") or 0),
            offset_x=int(body.get("offsetX") or 0),
            offset_y=int(body.get("offsetY") or 0),
            threshold=int(body.get("threshold") or 110),
        )
    elif source == "shape":
        pixels = bmpgen.render_shape(body.get("shape") or "circle", size=size)
    elif source == "pixels":
        pixels = bmpgen.normalise_pixels(body.get("pixels") or [], size)
    else:
        raise ValueError("Unbekannte Quelle: %s" % source)

    if body.get("post"):
        post = body["post"]
        pixels = bmpgen.transform(
            pixels,
            invert=bool(post.get("invert")),
            flip_h=bool(post.get("flipH")),
            flip_v=bool(post.get("flipV")),
            rotate=int(post.get("rotate") or 0),
            shift_x=int(post.get("shiftX") or 0),
            shift_y=int(post.get("shiftY") or 0),
        )

    return dict(pixels=pixels, size=size)


def api_bmp_download(body):
    size = int(body.get("size") or bmpgen.DEFAULT_SIZE)
    pixels = bmpgen.normalise_pixels(body.get("pixels") or [], size)
    data = bmpgen.pixels_to_bmp(pixels)
    return dict(name=(body.get("name") or "icon") + ".bmp",
                bytes=len(data),
                data=base64.b64encode(data).decode("ascii"))


def api_bmp_assign(body):
    pixels = body.get("pixels")
    if not pixels and body.get("id"):
        pixels = catalog.get_pixels(body["id"])
    if not pixels:
        raise ValueError("Kein Icon uebergeben")
    target = catalog.assign(STATE["sd_root"], body.get("profile"), body.get("slot"), pixels)
    return dict(written=target)


def api_catalog_list():
    return dict(entries=catalog.list_entries(), dir=catalog.CATALOG_DIR)


def api_catalog_save(body):
    return dict(entry=catalog.save(
        body.get("pixels") or [],
        body.get("name") or "Icon",
        tags=body.get("tags") or [],
        source=body.get("sourceLabel") or "webui",
        size=int(body.get("size") or bmpgen.DEFAULT_SIZE),
    ))


def api_sd():
    return dict(root=STATE["sd_root"], profiles=catalog.sd_profiles(STATE["sd_root"]))


ROUTES_GET = {
    "/api/meta": lambda q: api_meta(),
    "/api/config": lambda q: api_config_load((q.get("from") or ["repo"])[0]),
    "/api/catalog": lambda q: api_catalog_list(),
    "/api/sd": lambda q: api_sd(),
    "/api/mdi": lambda q: dict(status=mdi.status(),
                               icons=mdi.search((q.get("q") or [""])[0],
                                                int((q.get("limit") or ["200"])[0]))),
}

ROUTES_POST = {
    "/api/config/preview": api_config_preview,
    "/api/config/save": api_config_save,
    "/api/config/import": api_config_import,
    "/api/bmp/render": api_bmp_render,
    "/api/bmp/download": api_bmp_download,
    "/api/bmp/assign": api_bmp_assign,
    "/api/catalog/save": api_catalog_save,
    "/api/catalog/delete": lambda b: (catalog.delete(b["id"]), dict(ok=True))[1],
    "/api/catalog/rename": lambda b: dict(entry=catalog.rename(
        b["id"], b.get("name"), b.get("tags"))),
    "/api/catalog/import-sd": lambda b: dict(added=catalog.import_from_sd(STATE["sd_root"])),
    "/api/mdi/install": lambda b: dict(files=mdi.install_from_cdn(), status=mdi.status()),
    "/api/sd/root": lambda b: _set_sd_root(b.get("path")),
}


def _set_sd_root(path):
    path = (path or "").strip()
    if not path:
        raise ValueError("Kein Pfad angegeben")
    if not os.path.isdir(path):
        raise ValueError("Ordner existiert nicht: %s" % path)
    STATE["sd_root"] = os.path.abspath(path)
    return dict(root=STATE["sd_root"])


# --- HTTP -------------------------------------------------------------------

class Handler(BaseHTTPRequestHandler):
    server_version = "HapticPadStudio/1.0"

    def log_message(self, fmt, *args):  # ruhiger Server, Fehler kommen ueber send_error
        pass

    def _send(self, status, payload, content_type="application/json; charset=utf-8"):
        if isinstance(payload, (dict, list)):
            payload = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        elif isinstance(payload, str):
            payload = payload.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(payload)

    def _error(self, status, message):
        self._send(status, dict(error=str(message)))

    def _static(self, path):
        relative = path.lstrip("/") or "index.html"
        if relative.startswith("static/"):
            relative = relative[len("static/"):]
        target = os.path.normpath(os.path.join(STATIC_DIR, relative))
        if not target.startswith(STATIC_DIR) or not os.path.isfile(target):
            return self._error(404, "Nicht gefunden: %s" % path)
        mime = mimetypes.guess_type(target)[0] or "application/octet-stream"
        with open(target, "rb") as handle:
            self._send(200, handle.read(), mime)

    def do_GET(self):
        parsed = urlparse(self.path)

        if parsed.path.startswith("/api/"):
            handler = ROUTES_GET.get(parsed.path)
            if not handler:
                return self._error(404, "Unbekannte Route: %s" % parsed.path)
            try:
                return self._send(200, handler(parse_qs(parsed.query)))
            except Exception as error:  # jede Fehlerart landet lesbar in der UI
                return self._error(400, error)

        # Die MDI-Webfont liegt in assets/, der Browser holt sie fuer die
        # Icon-Vorschau von hier.
        if parsed.path == "/static/mdi.ttf":
            path = mdi.font_path()
            if not path:
                return self._error(404, "MDI-Schrift nicht installiert")
            with open(path, "rb") as handle:
                return self._send(200, handle.read(), "font/ttf")

        if parsed.path.startswith("/catalog/"):
            icon_id = os.path.basename(parsed.path)
            target = os.path.normpath(os.path.join(catalog.CATALOG_DIR, icon_id))
            if not target.startswith(catalog.CATALOG_DIR) or not os.path.isfile(target):
                return self._error(404, "Icon nicht gefunden")
            with open(target, "rb") as handle:
                return self._send(200, handle.read(), "image/bmp")

        return self._static(parsed.path)

    def do_POST(self):
        parsed = urlparse(self.path)
        handler = ROUTES_POST.get(parsed.path)
        if not handler:
            return self._error(404, "Unbekannte Route: %s" % parsed.path)

        length = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(length) if length else b"{}"
        try:
            body = json.loads(raw.decode("utf-8") or "{}")
        except ValueError as error:
            return self._error(400, "Ungueltiges JSON: %s" % error)

        try:
            return self._send(200, handler(body))
        except Exception as error:
            return self._error(400, error)


def main():
    parser = argparse.ArgumentParser(description="HapticPad Studio — WebUI")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--sd", default=STATE["sd_root"],
                        help="Ordner der SD-Karte (Standard: 'Example SD Card' im Repo)")
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args()

    STATE["sd_root"] = os.path.abspath(args.sd)
    os.makedirs(catalog.CATALOG_DIR, exist_ok=True)

    server = ThreadingHTTPServer((args.host, args.port), Handler)
    url = "http://%s:%d/" % (args.host, args.port)

    print("HapticPad Studio laeuft auf %s" % url)
    print("  Repo:      %s" % REPO)
    print("  SD-Karte:  %s" % STATE["sd_root"])
    print("  Katalog:   %s" % catalog.CATALOG_DIR)
    print("Beenden mit Strg+C.")

    if not args.no_browser:
        threading.Timer(0.5, lambda: webbrowser.open(url)).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nBeendet.")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
