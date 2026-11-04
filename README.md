# OpenCompanion-Firmware

Firmware ESP32-S3 du compagnon [OpenCompanion](https://github.com/AnthoDingo/OpenCompanion).

La carte se présente en USB comme un périphérique HID nommé « OpenCompanion ». Elle reçoit une
ligne de texte par statut (`OFFLINE`, `NOTEAMS` quand Teams est fermé, `AVAILABLE`, `AWAY`, `BRB`, `CALL`, `VIDEO`, `BUSY`, `DND`, `UPDATE` pendant une mise à jour du firmware),
éventuellement suivie du libellé localisé à afficher (`STATUT|Libellé`) et du numéro du compagnon
(`STATUT|Libellé|N` : 0 Pousse, 1 chouette, 2 magicien, 3 Trombone, 4 calculatrice, 5 panda), et anime le compagnon.
Le dernier compagnon reçu est mémorisé en flash et réaffiché au redémarrage.

Carte : **Waveshare ESP32-S3-LCD-1.69** (écran ST7789V2 240×280 intégré). Réglages carte/écran dans `platformio.ini`.

## Build / flash

```
pip install platformio
pio run            # compile
pio run -t upload  # flashe (maintenir BOOT puis rebrancher si la carte n'est pas trouvée)
```

Pour tester sans l'application : `pio device monitor`, puis envoyez `AVAILABLE`, `BUSY`, … `AVAILABLE|Disponible` ou `AVAILABLE|Disponible|2`.

## Release

Un tag `vX.Y` (= `FW_VERSION 0xXY0` dans `src/main.cpp`) **signe** et publie `firmware.bin` +
`firmware.bin.sig` en release GitHub ; l'application vérifie la signature puis l'installe via esptool.

## Signature (sécurité)

`firmware.bin` est signé (ECDSA P-256 / SHA-256) pour que l'application ne flashe que des binaires
de l'auteur. La CI signe avec openssl (runner Ubuntu) ; côté Windows, générez la clé avec .NET,
**sans openssl**. Mise en place, une fois, dans PowerShell :

```powershell
# 1. Générer la paire de clés dans signing.pem (garder secret, hors dépôt)
$ec = [System.Security.Cryptography.ECDsa]::Create([System.Security.Cryptography.ECCurve]::CreateFromFriendlyName("nistP256"))
[IO.File]::WriteAllText("$PWD\signing.pem", $ec.ExportPkcs8PrivateKeyPem())

# 2. Clé PUBLIQUE -> constante SigningPublicKey de l'appli (TeamsCompanion/Companion.cs)
[Convert]::ToBase64String($ec.ExportSubjectPublicKeyInfo())
```

Puis clé **privée** -> secret GitHub de CE dépôt, nommé `FIRMWARE_SIGNING_KEY` : colle le contenu
entier de `signing.pem` (`Get-Content signing.pem -Raw | Set-Clipboard`).

Tant que `SigningPublicKey` est vide côté appli, aucune mise à jour release n'est acceptée
(fail-closed). La clé privée ne doit jamais être commitée : elle vit seulement dans le secret CI.

Avec Git Bash (fourni par Git pour Windows), les commandes openssl marchent aussi :
`openssl ecparam -name prime256v1 -genkey -noout -out signing.pem` puis
`openssl ec -in signing.pem -pubout -outform DER | base64 -w0`.

## Sprites

`src/sprites.h` et `src/companions/<compagnon>.h` (un fichier par compagnon) sont **générés** depuis les PNG
du dépôt principal ([OpenCompanion](https://github.com/AnthoDingo/OpenCompanion), `sprites/png2h.py`). Pour modifier
un sprite, régénérez-les là-bas : le script écrit directement dans ce dépôt s'il est cloné à côté.
