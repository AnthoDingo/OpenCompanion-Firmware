// Généré par sprites/png2h.py (dépôt OpenCompanion), ne pas éditer à la main.
#pragma once
#include <stdint.h>

const int SPR_SIZE = 32;
const uint16_t SPR_PALETTE[] = {0x0000, 0x18E5, 0x3DAC, 0x238F, 0xA78E, 0xF7BE, 0x0842, 0xEBEA, 0x5370, 0x9598, 0x92E6, 0x59E4, 0xEE72, 0xFE45, 0x42D9, 0x298F, 0xFE54, 0xA5F1};
enum Frame : uint8_t { F_AWAY1, F_AWAY2, F_BACK, F_BLINK, F_CALL1, F_CALL2, F_IDLE, F_JUMP, F_LOOK, F_OFFLINE, F_SQUASH, F_COUNT };
enum Companion : uint8_t { C_POUSSE, C_CHOUETTE, C_MAGICIEN, C_TROMBONE, C_CALCULATRICE, C_PANDA, C_COUNT };

#include "companions/pousse.h"
#include "companions/chouette.h"
#include "companions/magicien.h"
#include "companions/trombone.h"
#include "companions/calculatrice.h"
#include "companions/panda.h"

// SPR_FRAMES[compagnon][frame] : pixels (index de SPR_PALETTE) de la frame, ligne par ligne.
const uint8_t (*const SPR_FRAMES[C_COUNT])[SPR_SIZE * SPR_SIZE] = {SPR_POUSSE, SPR_CHOUETTE, SPR_MAGICIEN, SPR_TROMBONE, SPR_CALCULATRICE, SPR_PANDA};
