# webui/assets

Hier liegen die Material-Design-Icons, damit der BMP-Generator daraus 15x15
Icons rendern kann. Beide Dateien sind bewusst nicht im Repo (siehe
`.gitignore`) — sie kommen von jsDelivr:

| Datei | Quelle |
| --- | --- |
| `materialdesignicons-webfont.ttf` | `https://cdn.jsdelivr.net/npm/@mdi/font@7.4.47/fonts/materialdesignicons-webfont.ttf` |
| `mdi-meta.json` | `https://cdn.jsdelivr.net/npm/@mdi/svg@7.4.47/meta.json` |

Zwei Wege, sie herzubekommen:

1. Im Reiter **BMP-Generator** auf *MDI-Icon* wechseln und dort den Knopf
   „Von jsDelivr laden“ druecken.
2. Beide Dateien von Hand herunterladen und hier ablegen.

Ohne diese Dateien funktioniert alles andere weiter — nur die MDI-Quelle
bleibt dann leer.
