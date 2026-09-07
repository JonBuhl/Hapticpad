# MacroPad 81f1b2d — 2026-09-08

## Beschreibung / Zustand des Codes
Coil-Whine-Fix: PWM-Carrier auf 50 kHz angehoben (MOTOR_PWM_FREQUENCY, gesetzt vor driver.init()). Erster Build des Fork-Stands ueberhaupt: enthaelt SmartKnob-Haptik-Engine (8 Wheel-Modi), Wheel-Domains mit Tap-Queue, Media/System/Hermes-Profile. Flash 174716 B (1%), RAM global 28832 B (10%).

## Git-Stand
- Branch: main
- Commit: 81f1b2d (81f1b2d81edda4bc344a960d8e0ddd866bd44731)
- Commit-Message: fix(haptics): raise PWM carrier to 50 kHz to silence coil whine
- Working tree: sauber

## Build
- FQBN: rp2040:rp2040:waveshare_rp2040_plus:flash=16777216_0,usbstack=tinyusb
- Core: rp2040 5.5.0
- Größe: 388096 Bytes
- SHA256: c65b8b336e1114bbd6fbd7c5a004cc5c1b57398be9b2648d993e48ba017039a9

## SD-Karte
(SD-Stand hier eintragen, falls relevant — z.B. config.xml-Version, Icons)

## Flashen
1. BOOTSEL gedrückt halten + USB einstecken → Laufwerk RPI-RP2 erscheint
2. MacroPad_81f1b2d.uf2 auf RPI-RP2 kopieren
3. Board neu einstecken → Firmware startet
