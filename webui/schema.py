#!/usr/bin/env python3
"""Schema der HapticPad-Konfiguration.

Einzige Quelle fuer das, was die WebUI anbietet: Wertebereiche, Defaults und
Keycodes. Die Grenzen spiegeln validateHapticSettings() aus xmlData.ino wider,
damit die UI nichts anbieten kann, was die Firmware anschliessend wegkuerzt.
"""

# --- Modi -------------------------------------------------------------------

# Reihenfolge = Index in wheelModeNames[] (haptics.h)
WHEEL_MODES = [
    "Clicky", "Twist", "Momentum", "Free", "Endstop", "Friction", "Snap", "Magnetic",
]

# Reihenfolge = Index in ledModeNames[] (MacroPad.ino)
LED_MODES = ["Halo", "Breath", "Bands", "Rainbow", "Solid", "Off"]

WHEEL_MODE_HELP = {
    "Clicky": "Rasten rundum, unbegrenzt drehbar. Clicky_Detents / Clicky_Strength.",
    "Twist": "Federt in die Mitte zurueck, begrenzter Winkel. Twist_Range / Twist_Strength.",
    "Momentum": "Freilauf mit Nachlauf, benutzt noch die alte PID (Momentum_P/I).",
    "Free": "Kein Drehmoment, reines Scrollen.",
    "Endstop": "Begrenzter Bereich mit Anschlaegen. Haptic_Range / Haptic_EndstopStrength.",
    "Friction": "Gleichmaessiger Widerstand ohne Rasten. Friction_Strength.",
    "Snap": "Springt auf die naechste Position. Snap_Detents / Snap_Point.",
    "Magnetic": "Rasten nur an ausgewaehlten Positionen. Haptic_MagneticPositions.",
}

LED_MODE_HELP = {
    "Halo": "Lauflicht, verlaeuft von Primaer- nach Sekundaerfarbe.",
    "Breath": "Atmen zwischen Primaer- und Sekundaerfarbe.",
    "Bands": "Abwechselnde Segmente in Primaer-/Sekundaerfarbe.",
    "Rainbow": "Durchlaufender Farbkreis, die eigenen Farben wirken hier nicht.",
    "Solid": "Konstant Primaerfarbe.",
    "Off": "Ring aus.",
}

# --- Settings ---------------------------------------------------------------
# Die Firmware sucht die Tags streng der Reihe nach im Dateistrom (File::find),
# ein Umsortieren zerlegt das Parsen. Diese Liste ist die verbindliche Reihenfolge.
# kind: float | int | rgb | enum | modelist | csv
SETTINGS = [
    dict(name="LED_Mode", kind="enum", options=LED_MODES, default="Solid",
         group="LED", label="LED-Modus",
         help="Modus beim Start. Am Geraet ueber das Menue umschaltbar."),
    dict(name="LED_Primary", kind="rgb", default=[255, 0, 0],
         group="LED", label="Primaerfarbe", help="R,G,B 0-255."),
    dict(name="LED_Secondary", kind="rgb", default=[0, 0, 255],
         group="LED", label="Sekundaerfarbe",
         help="Zweite Farbe fuer Halo, Breath und Bands."),
    dict(name="LED_Menu", kind="modelist", options=LED_MODES, default=list(LED_MODES),
         group="LED", label="Modi im Geraetemenue",
         help="Welche LED-Modi am Geraet durchgeschaltet werden koennen. Leere Auswahl = alle."),

    dict(name="Clicky_P", kind="float", min=0.0, max=5.0, step=0.05, default=0.5,
         group="Legacy-PID", label="Clicky P", legacy=True,
         help="Alt-PID. Clicky laeuft ueber das Haptik-Modell, der Wert wird nur "
              "noch eingelesen damit alte Configs laden."),
    dict(name="Clicky_I", kind="float", min=0.0, max=5.0, step=0.05, default=0.0,
         group="Legacy-PID", label="Clicky I", legacy=True),
    dict(name="Twist_P", kind="float", min=0.0, max=5.0, step=0.05, default=0.65,
         group="Legacy-PID", label="Twist P", legacy=True),
    dict(name="Twist_I", kind="float", min=0.0, max=5.0, step=0.05, default=0.2,
         group="Legacy-PID", label="Twist I", legacy=True),
    dict(name="Momentum_P", kind="float", min=0.0, max=5.0, step=0.05, default=0.3,
         group="Legacy-PID", label="Momentum P",
         help="Momentum benutzt diese PID weiterhin aktiv."),
    dict(name="Momentum_I", kind="float", min=0.0, max=5.0, step=0.05, default=0.0,
         group="Legacy-PID", label="Momentum I",
         help="Momentum benutzt diese PID weiterhin aktiv."),

    dict(name="Haptic_VoltageLimit", kind="float", min=0.5, max=5.0, step=0.1, default=3.0,
         group="Haptik global", label="Spannungsgrenze (V)",
         help="Drehmoment-Obergrenze des Motors. Hoeher = kraeftiger und waermer."),
    dict(name="Haptic_DetentStrength", kind="float", min=0.0, max=5.0, step=0.1, default=2.5,
         group="Haptik global", label="Rastenstaerke",
         help="Grundstaerke der Rasten. Setzt beim Laden auch Clicky_Strength, "
              "sofern dieses Tag danach nicht eigens gesetzt wird."),
    dict(name="Haptic_EndstopStrength", kind="float", min=0.0, max=5.0, step=0.1, default=3.0,
         group="Haptik global", label="Anschlagstaerke",
         help="Kraft an den Enden begrenzter Modi (Endstop, Twist)."),
    dict(name="Haptic_SnapPoint", kind="float", min=0.55, max=2.0, step=0.05, default=1.1,
         group="Haptik global", label="Snap-Punkt",
         help=">1 = Klick ueber den Punkt hinweg, 0.55 = springt zur naechsten Rast."),
    dict(name="Haptic_Range", kind="int", min=2, max=255, default=24,
         group="Haptik global", label="Endstop-Bereich (Positionen)",
         help="Anzahl Positionen zwischen den Anschlaegen im Endstop-Modus."),
    dict(name="Haptic_MagneticPositions", kind="csv", max_items=16, item_max=255,
         default="0,6,12,18",
         group="Haptik global", label="Magnetische Positionen",
         help="Bis zu 16 Positionen, an denen der Magnetic-Modus rastet. Werte ab "
              "Magnetic_Detents verwirft die Firmware."),

    dict(name="Clicky_Detents", kind="int", min=4, max=400, default=40,
         group="Wheel-Modi", label="Clicky: Rasten pro Umdrehung"),
    dict(name="Clicky_Strength", kind="float", min=0.0, max=5.0, step=0.1, default=2.0,
         group="Wheel-Modi", label="Clicky: Staerke"),
    dict(name="Twist_Strength", kind="float", min=0.0, max=5.0, step=0.1, default=2.0,
         group="Wheel-Modi", label="Twist: Rueckstellkraft"),
    dict(name="Twist_Range", kind="float", min=5.0, max=180.0, step=1.0, default=60.0,
         group="Wheel-Modi", label="Twist: Auslenkung (Grad)"),
    dict(name="Friction_Strength", kind="float", min=0.0, max=5.0, step=0.1, default=1.0,
         group="Wheel-Modi", label="Friction: Widerstand"),
    dict(name="Snap_Strength", kind="float", min=0.0, max=5.0, step=0.1, default=3.0,
         group="Wheel-Modi", label="Snap: Staerke"),
    dict(name="Snap_Detents", kind="int", min=4, max=400, default=20,
         group="Wheel-Modi", label="Snap: Positionen"),
    dict(name="Snap_Point", kind="float", min=0.55, max=2.0, step=0.05, default=0.55,
         group="Wheel-Modi", label="Snap: Snap-Punkt"),
    dict(name="Magnetic_Strength", kind="float", min=0.0, max=5.0, step=0.1, default=2.5,
         group="Wheel-Modi", label="Magnetic: Staerke"),
    dict(name="Magnetic_Detents", kind="int", min=4, max=255, default=24,
         group="Wheel-Modi", label="Magnetic: Positionen"),

    # Bewusst als letztes: ein fehlendes Tag laesst find() bis Dateiende lesen
    # und wuerde jedes danach gesuchte Tag verhungern lassen.
    dict(name="Sleep_Timeout", kind="int", min=0, max=240, default=5,
         group="Sonstiges", label="Sleep-Timeout (Minuten)",
         help="Minuten ohne Eingabe bis OLED und LED-Ring ausgehen. 0 = aus."),
]

SETTINGS_ORDER = [s["name"] for s in SETTINGS]
SETTINGS_BY_NAME = {s["name"]: s for s in SETTINGS}

ACTIONS_PER_BUTTON = 3
BUTTONS_PER_PROFILE = 6

# --- Keycodes ---------------------------------------------------------------
# Werte wie in keyCodes.ino / convertConsumerKeycode(). Was hier nicht steht,
# sendet die Firmware nicht.
KEYCODES = [
    (0, "— keine —", "Leer"),

    (16, "Shift", "Modifier"),
    (17, "Strg", "Modifier"),
    (18, "Alt", "Modifier"),
    (91, "GUI / Windows links", "Modifier"),
    (92, "GUI / Windows rechts", "Modifier"),

    (8, "Backspace", "Steuerung"),
    (9, "Tab", "Steuerung"),
    (13, "Enter", "Steuerung"),
    (19, "Pause", "Steuerung"),
    (20, "Feststell", "Steuerung"),
    (27, "Esc", "Steuerung"),
    (32, "Leertaste", "Steuerung"),
    (33, "Bild auf", "Steuerung"),
    (34, "Bild ab", "Steuerung"),
    (35, "Ende", "Steuerung"),
    (36, "Pos1", "Steuerung"),
    (44, "Druck / Screenshot", "Steuerung"),
    (45, "Einfg", "Steuerung"),
    (46, "Entf", "Steuerung"),
    (144, "Num-Lock", "Steuerung"),
    (145, "Rollen", "Steuerung"),

    (37, "Pfeil links", "Navigation"),
    (38, "Pfeil hoch", "Navigation"),
    (39, "Pfeil rechts", "Navigation"),
    (40, "Pfeil runter", "Navigation"),
]

KEYCODES += [(48 + i, "Ziffer %d" % i, "Ziffern") for i in range(10)]
KEYCODES += [(65 + i, chr(65 + i), "Buchstaben") for i in range(26)]
KEYCODES += [(112 + i, "F%d" % (i + 1), "Funktionstasten") for i in range(12)]
KEYCODES += [(96 + i, "Numpad %d" % i, "Numpad") for i in range(10)]
KEYCODES += [
    (106, "Numpad *", "Numpad"),
    (107, "Numpad +", "Numpad"),
    (109, "Numpad -", "Numpad"),
    (110, "Numpad ,", "Numpad"),
    (111, "Numpad /", "Numpad"),

    (186, "; (OEM 1)", "Zeichen"),
    (187, "= / +", "Zeichen"),
    (188, ", (Komma)", "Zeichen"),
    (189, "- (Minus)", "Zeichen"),
    (190, ". (Punkt)", "Zeichen"),
    (191, "/ (Slash)", "Zeichen"),
    (192, "` (Grave / ^)", "Zeichen"),
    (219, "[ (Bracket links)", "Zeichen"),
    (220, "\\ (Backslash)", "Zeichen"),
    (221, "] (Bracket rechts)", "Zeichen"),
    (222, "' (Apostroph)", "Zeichen"),

    (173, "Stumm", "Media"),
    (174, "Lautstaerke -", "Media"),
    (175, "Lautstaerke +", "Media"),
    (176, "Play", "Media"),
    (177, "Pause", "Media"),
    (179, "Vorspulen", "Media"),
    (180, "Zurueckspulen", "Media"),
    (181, "Naechster Titel", "Media"),
    (182, "Voriger Titel", "Media"),
    (183, "Stop", "Media"),
    (184, "Helligkeit +", "Media"),
    (185, "Helligkeit -", "Media"),
    (205, "Play / Pause", "Media"),
]

VALID_KEYCODES = set(code for code, _, _ in KEYCODES)
KEYCODE_NAMES = dict((code, name) for code, name, _ in KEYCODES)


def clamp(value, low, high):
    return low if value < low else (high if value > high else value)


def coerce_setting(name, value):
    """Wert auf Typ und Bereich bringen, den die Firmware akzeptiert."""
    spec = SETTINGS_BY_NAME[name]
    kind = spec["kind"]

    if kind == "enum":
        return value if value in spec["options"] else spec["default"]

    if kind == "rgb":
        if isinstance(value, str):
            value = [p.strip() for p in value.split(",")]
        parts = [int(clamp(int(float(p)), 0, 255)) for p in list(value)[:3]]
        while len(parts) < 3:
            parts.append(0)
        return parts

    if kind == "modelist":
        seen, out = set(), []
        for mode in value or []:
            if mode in spec["options"] and mode not in seen:
                seen.add(mode)
                out.append(mode)
        return out or list(spec["options"])

    if kind == "csv":
        raw = value if isinstance(value, (list, tuple)) else str(value).replace(";", ",").split(",")
        out = []
        for part in raw:
            part = str(part).strip()
            if not part:
                continue
            try:
                number = int(float(part))
            except ValueError:
                continue
            number = int(clamp(number, 0, spec["item_max"]))
            if number not in out:
                out.append(number)
        return ",".join(str(n) for n in out[:spec["max_items"]])

    if kind == "int":
        return int(clamp(int(round(float(value))), spec["min"], spec["max"]))

    return round(float(clamp(float(value), spec["min"], spec["max"])), 4)


def coerce_keycode(value):
    try:
        code = int(float(value))
    except (TypeError, ValueError):
        return 0
    return code if code in VALID_KEYCODES else 0


def coerce_delay(value):
    try:
        delay = int(float(value))
    except (TypeError, ValueError):
        return 0
    return int(clamp(delay, 0, 255))


def default_settings():
    out = {}
    for spec in SETTINGS:
        default = spec["default"]
        out[spec["name"]] = list(default) if isinstance(default, list) else default
    return out


def default_button(index=1):
    return dict(
        label="Taste %d" % index,
        actions=[[0, 0], [0, 0], [0, 0]],
        wheelMode=None,
        wheelUp=None,
        wheelDown=None,
    )


def default_profile(name="Neues Profil"):
    return dict(
        name=name,
        WheelMode="Clicky",
        WheelKey=0,
        buttons=[default_button(i + 1) for i in range(BUTTONS_PER_PROFILE)],
    )
