#!/usr/bin/env python3
"""Erzeugt config.xml aus config.yaml.

config.yaml ist die Quelle der Wahrheit, die Firmware liest ausschliesslich die
XML. Das Skript schreibt beide Kopien (Wurzel und "Example SD Card"), damit sie
byte-identisch bleiben.

Aufruf:  python3 scripts/yaml_to_xml.py
"""

import os
import sys
from xml.sax.saxutils import escape

import yaml

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(REPO, "config.yaml")
TARGETS = [
    os.path.join(REPO, "config.xml"),
    os.path.join(REPO, "Example SD Card", "config.xml"),
]

# Reihenfolge der Settings-Tags. Die Firmware sucht die Tags streng der Reihe
# nach im Dateistrom, ein Umsortieren wuerde das Parsen zerlegen.
SETTINGS_ORDER = [
    "LED_Mode",
    "LED_Primary",
    "LED_Secondary",
    "LED_Menu",
    "Clicky_P",
    "Clicky_I",
    "Twist_P",
    "Twist_I",
    "Momentum_P",
    "Momentum_I",
    "Haptic_VoltageLimit",
    "Haptic_DetentStrength",
    "Haptic_EndstopStrength",
    "Haptic_SnapPoint",
    "Haptic_Range",
    "Haptic_MagneticPositions",
    "Clicky_Detents",
    "Clicky_Strength",
    "Twist_Strength",
    "Twist_Range",
    "Friction_Strength",
    "Snap_Strength",
    "Snap_Detents",
    "Snap_Point",
    "Magnetic_Strength",
    "Magnetic_Detents",
]

WHEEL_MODES = [
    "Clicky",
    "Twist",
    "Momentum",
    "Free",
    "Endstop",
    "Friction",
    "Snap",
    "Magnetic",
]

ACTIONS_PER_BUTTON = 3
BUTTONS_PER_PROFILE = 6


def scalar(value):
    """Zahl/Text so formatieren, wie die Firmware sie erwartet (3.0 -> 3)."""
    if isinstance(value, bool):
        raise ValueError("Boolesche Settings-Werte werden nicht unterstuetzt")
    if isinstance(value, float):
        return "%g" % value
    return str(value)


def action(pair):
    """Eine Aktion [Delay, Keycode] als "delay,key"."""
    delay, key = pair
    return "%s,%s" % (scalar(delay), scalar(key))


def settings_lines(settings):
    lines = []

    for name in SETTINGS_ORDER:
        if name not in settings:
            continue

        value = settings[name]

        if name == "LED_Menu":
            lines.append("    <LED_Menu>")
            for mode in value:
                lines.append("      <Mode>%s</Mode>" % escape(str(mode)))
            lines.append("    </LED_Menu>")
        elif isinstance(value, list):
            lines.append(
                "    <%s>%s</%s>" % (name, ",".join(scalar(v) for v in value), name)
            )
        else:
            lines.append("    <%s>%s</%s>" % (name, escape(scalar(value)), name))

    unknown = [key for key in settings if key not in SETTINGS_ORDER]
    if unknown:
        raise ValueError("Unbekannte Settings: %s" % ", ".join(sorted(unknown)))

    return lines


def button_lines(button):
    lines = ["        <MacroButton>"]

    # Ein Wheel-Domain-Button (wheelMode gesetzt) schaltet nur das Rad um, seine
    # Aktionen werden von der Firmware ignoriert. Die drei Action-Tags bleiben
    # trotzdem stehen, damit die Struktur fuer alle Buttons gleich ist.
    actions = button.get("actions") or []
    for index in range(ACTIONS_PER_BUTTON):
        pair = actions[index] if index < len(actions) else [0, 0]
        lines.append("          <Action>%s</Action>" % action(pair))

    lines.append("          <Label>%s</Label>" % escape(str(button.get("label", ""))))

    wheel_mode = button.get("wheelMode")
    if wheel_mode is not None:
        if wheel_mode not in WHEEL_MODES:
            raise ValueError("Unbekannter wheelMode: %s" % wheel_mode)
        lines.append("          <WheelMode>%s</WheelMode>" % wheel_mode)

    for key, tag in (("wheelUp", "WheelUp"), ("wheelDown", "WheelDown")):
        value = button.get(key)
        if value is None:
            continue
        if wheel_mode is None:
            raise ValueError("%s ohne wheelMode bei Button '%s'" % (key, button.get("label")))
        lines.append("          <%s>%s</%s>" % (tag, action(value), tag))

    modifier = button.get("wheelMod")
    if modifier is not None:
        if wheel_mode is None:
            raise ValueError("wheelMod ohne wheelMode bei Button '%s'" % button.get("label"))
        lines.append("          <WheelMod>%s</WheelMod>" % scalar(modifier))

    lines.append("        </MacroButton>")
    return lines


def profile_lines(profile):
    lines = ['    <Profile name="%s">' % escape(str(profile["name"]), {'"': "&quot;"})]
    lines.append("      <WheelMode>%s</WheelMode>" % profile["WheelMode"])
    lines.append("      <WheelKey>%s</WheelKey>" % scalar(profile.get("WheelKey", 0)))
    lines.append("      <MacroButtons>")

    buttons = profile.get("buttons") or []
    if len(buttons) != BUTTONS_PER_PROFILE:
        raise ValueError(
            "Profil '%s' hat %d Buttons, erwartet %d"
            % (profile["name"], len(buttons), BUTTONS_PER_PROFILE)
        )

    for button in buttons:
        lines.extend(button_lines(button))

    lines.append("      </MacroButtons>")
    lines.append("    </Profile>")
    return lines


def build_xml(config):
    lines = ['<?xml version="1.0" encoding="UTF-8"?>', "<Configuration>", "  <Settings>"]
    lines.extend(settings_lines(config["Settings"]))
    lines.append("  </Settings>")
    lines.append("  <Profiles>")

    for profile in config["Profiles"]:
        lines.extend(profile_lines(profile))

    lines.append("  </Profiles>")
    lines.append("</Configuration>")
    return "\n".join(lines) + "\n"


def main():
    with open(SOURCE, "r", encoding="utf-8") as handle:
        config = yaml.safe_load(handle)

    xml = build_xml(config)

    for target in TARGETS:
        with open(target, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(xml)
        print("geschrieben: %s" % os.path.relpath(target, REPO))

    print("%d Profile" % len(config["Profiles"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
