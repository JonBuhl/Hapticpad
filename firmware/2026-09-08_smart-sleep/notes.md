# MacroPad smart-sleep — 2026-09-08

## Beschreibung / Zustand des Codes
Smart Sleep: OLED (u8g2 setPowerSave) und LED-Ring gehen nach <Sleep_Timeout> Minuten ohne Eingabe aus, jeder Tastendruck weckt beides. Der weckende Druck wird geschluckt (kein Makro, kein Profilwechsel). Rad haelt wach, weckt aber nicht. Motor/Haptik/Scrollen laufen im Schlaf weiter. Neuer Settings-Tag <Sleep_Timeout> (Default 5, 0 = aus, max 240), muss letzter Tag in <Settings> bleiben. Flash 175540 B (1%), RAM global 28844 B (11%).

## Git-Stand
- Branch: main
- Commit: 0621be3 (0621be39bf63003db3de9146e22c7adaaec2a854)
- Commit-Message: chore(firmware): version the firmware archive and add first main build
- Working tree:  M .gitignore
 M "Example SD Card/config.xml"
 M README.md
 M Software/MacroPad/MacroPad.ino
 M Software/MacroPad/xmlData.ino

## Build
- FQBN: rp2040:rp2040:waveshare_rp2040_plus:flash=16777216_0,usbstack=tinyusb
- Core: rp2040 5.5.0
- Größe: 390144 Bytes
- SHA256: af35021bacb48adab928af32103c57ecc95f11edd59d2ef8c1b7704a6f44561a

## SD-Karte
(SD-Stand hier eintragen, falls relevant — z.B. config.xml-Version, Icons)

## Flashen
1. BOOTSEL gedrückt halten + USB einstecken → Laufwerk RPI-RP2 erscheint
2. MacroPad_smart-sleep.uf2 auf RPI-RP2 kopieren
3. Board neu einstecken → Firmware startet
