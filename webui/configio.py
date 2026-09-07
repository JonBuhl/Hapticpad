#!/usr/bin/env python3
"""XML/YAML fuer die HapticPad-Konfiguration lesen und schreiben.

Die Firmware parst die XML nicht als Baum, sondern sucht Tags der Reihe nach im
Dateistrom. Der Writer haelt sich deshalb strikt an SETTINGS_ORDER und schreibt
pro Profil immer 6 Buttons mit je 3 Actions, auch wenn Werte 0 sind.
"""

import os
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape

import schema


# --- Schreiben --------------------------------------------------------------

def _scalar(value):
    """Zahl so formatieren, wie die Firmware sie erwartet (3.0 -> 3)."""
    if isinstance(value, bool):
        raise ValueError("Boolesche Werte sind nicht vorgesehen")
    if isinstance(value, float):
        return "%g" % value
    return str(value)


def _action(pair):
    delay, key = pair
    return "%s,%s" % (_scalar(delay), _scalar(key))


def _settings_lines(settings):
    lines = []
    for name in schema.SETTINGS_ORDER:
        if name not in settings:
            continue
        value = schema.coerce_setting(name, settings[name])
        spec = schema.SETTINGS_BY_NAME[name]

        if spec["kind"] == "modelist":
            lines.append("    <LED_Menu>")
            for mode in value:
                lines.append("      <Mode>%s</Mode>" % escape(str(mode)))
            lines.append("    </LED_Menu>")
        elif isinstance(value, list):
            lines.append("    <%s>%s</%s>" % (name, ",".join(_scalar(v) for v in value), name))
        else:
            lines.append("    <%s>%s</%s>" % (name, escape(_scalar(value)), name))
    return lines


def _button_lines(button):
    lines = ["        <MacroButton>"]

    # Ein Wheel-Domain-Button schaltet nur das Rad um, seine Actions ignoriert
    # die Firmware. Die drei Tags bleiben trotzdem stehen, damit die Struktur
    # fuer alle Buttons gleich ist.
    actions = button.get("actions") or []
    for index in range(schema.ACTIONS_PER_BUTTON):
        pair = actions[index] if index < len(actions) else [0, 0]
        pair = [schema.coerce_delay(pair[0]), schema.coerce_keycode(pair[1])]
        lines.append("          <Action>%s</Action>" % _action(pair))

    lines.append("          <Label>%s</Label>" % escape(str(button.get("label", ""))))

    wheel_mode = button.get("wheelMode") or None
    if wheel_mode:
        if wheel_mode not in schema.WHEEL_MODES:
            raise ValueError("Unbekannter wheelMode: %s" % wheel_mode)
        lines.append("          <WheelMode>%s</WheelMode>" % wheel_mode)

        for key, tag in (("wheelUp", "WheelUp"), ("wheelDown", "WheelDown")):
            pair = button.get(key)
            if not pair:
                continue
            pair = [schema.coerce_delay(pair[0]), schema.coerce_keycode(pair[1])]
            lines.append("          <%s>%s</%s>" % (tag, _action(pair), tag))

    lines.append("        </MacroButton>")
    return lines


def _profile_lines(profile):
    name = str(profile.get("name", "")).strip() or "Profil"
    wheel_mode = profile.get("WheelMode", "Clicky")
    if wheel_mode not in schema.WHEEL_MODES:
        raise ValueError("Profil '%s': unbekannter WheelMode '%s'" % (name, wheel_mode))

    lines = ['    <Profile name="%s">' % escape(name, {'"': "&quot;"})]
    lines.append("      <WheelMode>%s</WheelMode>" % wheel_mode)
    lines.append("      <WheelKey>%s</WheelKey>" % schema.coerce_keycode(profile.get("WheelKey", 0)))
    lines.append("      <MacroButtons>")

    buttons = list(profile.get("buttons") or [])
    while len(buttons) < schema.BUTTONS_PER_PROFILE:
        buttons.append(schema.default_button(len(buttons) + 1))

    for button in buttons[:schema.BUTTONS_PER_PROFILE]:
        lines.extend(_button_lines(button))

    lines.append("      </MacroButtons>")
    lines.append("    </Profile>")
    return lines


def build_xml(config):
    lines = ['<?xml version="1.0" encoding="UTF-8"?>', "<Configuration>", "  <Settings>"]
    lines.extend(_settings_lines(config.get("Settings") or schema.default_settings()))
    lines.append("  </Settings>")
    lines.append("  <Profiles>")
    for profile in config.get("Profiles") or []:
        lines.extend(_profile_lines(profile))
    lines.append("  </Profiles>")
    lines.append("</Configuration>")
    return "\n".join(lines) + "\n"


def build_yaml(config):
    """config.yaml im Stil der bestehenden Datei, ohne PyYAML-Abhaengigkeit."""
    out = ["# Von der HapticPad WebUI erzeugt (webui/app.py).",
           "# Quelle der Wahrheit bleibt die XML, diese YAML ist die lesbare Kopie.",
           "",
           "Settings:"]

    settings = config.get("Settings") or schema.default_settings()
    for name in schema.SETTINGS_ORDER:
        if name not in settings:
            continue
        value = schema.coerce_setting(name, settings[name])
        spec = schema.SETTINGS_BY_NAME[name]
        if spec["kind"] == "modelist":
            out.append('  %s: [%s]' % (name, ", ".join('"%s"' % m for m in value)))
        elif spec["kind"] == "rgb":
            out.append("  %s: [%s]" % (name, ", ".join(str(v) for v in value)))
        elif spec["kind"] == "enum":
            out.append('  %s: "%s"' % (name, value))
        elif spec["kind"] == "csv":
            out.append('  %s: "%s"' % (name, value))
        else:
            out.append("  %s: %s" % (name, _scalar(value)))

    out.append("")
    out.append("Profiles:")
    for profile in config.get("Profiles") or []:
        out.append('  - name: "%s"' % str(profile.get("name", "")).replace('"', "'"))
        out.append('    WheelMode: "%s"' % profile.get("WheelMode", "Clicky"))
        out.append("    WheelKey: %d" % schema.coerce_keycode(profile.get("WheelKey", 0)))
        out.append("    buttons:")
        buttons = list(profile.get("buttons") or [])
        while len(buttons) < schema.BUTTONS_PER_PROFILE:
            buttons.append(schema.default_button(len(buttons) + 1))
        for button in buttons[:schema.BUTTONS_PER_PROFILE]:
            actions = list(button.get("actions") or [])
            while len(actions) < schema.ACTIONS_PER_BUTTON:
                actions.append([0, 0])
            pairs = ", ".join(
                "[%d, %d]" % (schema.coerce_delay(a[0]), schema.coerce_keycode(a[1]))
                for a in actions[:schema.ACTIONS_PER_BUTTON]
            )
            out.append('      - label: "%s"' % str(button.get("label", "")).replace('"', "'"))
            out.append("        actions: [%s]" % pairs)
            if button.get("wheelMode"):
                out.append('        wheelMode: "%s"' % button["wheelMode"])
                for key, tag in (("wheelUp", "wheelUp"), ("wheelDown", "wheelDown")):
                    pair = button.get(key)
                    if pair:
                        out.append("        %s: [%d, %d]" % (
                            tag, schema.coerce_delay(pair[0]), schema.coerce_keycode(pair[1])))

    return "\n".join(out) + "\n"


# --- Lesen ------------------------------------------------------------------

def _parse_action(text):
    if not text:
        return [0, 0]
    parts = [p.strip() for p in str(text).split(",")]
    delay = schema.coerce_delay(parts[0]) if parts else 0
    key = schema.coerce_keycode(parts[1]) if len(parts) > 1 else 0
    return [delay, key]


def parse_xml(text):
    """config.xml einlesen. Unbekannte Tags werden still ignoriert."""
    root = ET.fromstring(text)

    settings = schema.default_settings()
    node = root.find("Settings")
    if node is not None:
        for spec in schema.SETTINGS:
            name = spec["name"]
            if spec["kind"] == "modelist":
                menu = node.find("LED_Menu")
                if menu is not None:
                    modes = [(m.text or "").strip() for m in menu.findall("Mode")]
                    settings[name] = schema.coerce_setting(name, modes)
                continue
            child = node.find(name)
            if child is None or child.text is None:
                continue
            settings[name] = schema.coerce_setting(name, child.text.strip())

    profiles = []
    holder = root.find("Profiles")
    for pnode in (holder.findall("Profile") if holder is not None else []):
        wheel_mode = (pnode.findtext("WheelMode") or "Clicky").strip()
        profile = dict(
            name=pnode.get("name", "Profil"),
            WheelMode=wheel_mode if wheel_mode in schema.WHEEL_MODES else "Clicky",
            WheelKey=schema.coerce_keycode(pnode.findtext("WheelKey") or 0),
            buttons=[],
        )

        buttons_node = pnode.find("MacroButtons")
        for bnode in (buttons_node.findall("MacroButton") if buttons_node is not None else []):
            actions = [_parse_action(a.text) for a in bnode.findall("Action")]
            while len(actions) < schema.ACTIONS_PER_BUTTON:
                actions.append([0, 0])

            button = dict(
                label=(bnode.findtext("Label") or "").strip(),
                actions=actions[:schema.ACTIONS_PER_BUTTON],
                wheelMode=None,
                wheelUp=None,
                wheelDown=None,
            )

            domain = (bnode.findtext("WheelMode") or "").strip()
            if domain in schema.WHEEL_MODES:
                button["wheelMode"] = domain
                up = bnode.findtext("WheelUp")
                down = bnode.findtext("WheelDown")
                if up:
                    button["wheelUp"] = _parse_action(up)
                if down:
                    button["wheelDown"] = _parse_action(down)

            profile["buttons"].append(button)

        while len(profile["buttons"]) < schema.BUTTONS_PER_PROFILE:
            profile["buttons"].append(schema.default_button(len(profile["buttons"]) + 1))
        profile["buttons"] = profile["buttons"][:schema.BUTTONS_PER_PROFILE]
        profiles.append(profile)

    return dict(Settings=settings, Profiles=profiles)


def load_xml_file(path):
    with open(path, "r", encoding="utf-8") as handle:
        return parse_xml(handle.read())


def write_text(path, text):
    directory = os.path.dirname(path)
    if directory:
        os.makedirs(directory, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


# --- Pruefungen -------------------------------------------------------------

def lint(config):
    """Sammelt Hinweise, die die Firmware sonst still wegkuerzen wuerde."""
    notes = []
    settings = config.get("Settings") or {}
    profiles = config.get("Profiles") or []

    positions = str(schema.coerce_setting(
        "Haptic_MagneticPositions", settings.get("Haptic_MagneticPositions", "")))
    detents = schema.coerce_setting("Magnetic_Detents", settings.get("Magnetic_Detents", 24))
    dropped = [p for p in positions.split(",") if p and int(p) >= detents]
    if dropped:
        notes.append("Magnetische Positionen %s liegen ausserhalb von Magnetic_Detents "
                     "(%d) und werden von der Firmware verworfen."
                     % (", ".join(dropped), detents))

    if not profiles:
        notes.append("Keine Profile angelegt — das Geraet haette nichts zu laden.")

    seen = set()
    for profile in profiles:
        name = str(profile.get("name", "")).strip()
        if not name:
            notes.append("Ein Profil hat keinen Namen.")
        elif name in seen:
            notes.append("Profilname '%s' kommt mehrfach vor — beide zeigen dieselben "
                         "Icons vom gleichen SD-Ordner." % name)
        else:
            seen.add(name)

        if len(name) > 15:
            notes.append("Profilname '%s' ist laenger als 15 Zeichen und wird auf dem "
                         "Display abgeschnitten." % name)

        for index, button in enumerate(profile.get("buttons") or [], start=1):
            if len(str(button.get("label", ""))) > 12:
                notes.append("%s / Taste %d: Label '%s' ist fuer das Display recht lang."
                             % (name, index, button.get("label")))
            if button.get("wheelMode") and any(a[1] for a in (button.get("actions") or [])):
                notes.append("%s / Taste %d ist ein Wheel-Domain-Button, die eingetragenen "
                             "Tastendruecke ignoriert die Firmware." % (name, index))

    return notes
