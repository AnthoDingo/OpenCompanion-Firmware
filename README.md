# OpenCompanion-Firmware

Firmware ESP32-S3 du compagnon [OpenCompanion](https://github.com/AnthoDingo/OpenCompanion).

La carte se présente en USB comme un périphérique HID nommé « OpenCompanion ». Elle reçoit une
ligne de texte par statut (`OFFLINE`, `AVAILABLE`, `AWAY`, `CALL`, `VIDEO`, `BUSY`, `DND`),
éventuellement suivie du libellé localisé à afficher (`STATUT|Libellé`), et anime le compagnon.

Carte : **Waveshare ESP32-S3-LCD-1.69** (écran ST7789V2 240×280 intégré). Réglages carte/écran dans `platformio.ini`.

## Build / flash

```
pip install platformio
pio run            # compile
pio run -t upload  # flashe (maintenir BOOT puis rebrancher si la carte n'est pas trouvée)
```

Pour tester sans l'application : `pio device monitor`, puis envoyez `AVAILABLE`, `BUSY`, … ou `AVAILABLE|Disponible`.

## Release

Un tag `vX.Y` (= `FW_VERSION 0xXY0` dans `src/main.cpp`) publie `firmware.bin` en release GitHub ;
l'application l'installe automatiquement via esptool.

## Sprites

`src/sprites.h` est **généré** depuis les PNG du dépôt principal
([OpenCompanion](https://github.com/AnthoDingo/OpenCompanion), `sprites/png2h.py`). Pour modifier un
sprite, régénérez-le là-bas et recopiez `sprites.h` ici.
