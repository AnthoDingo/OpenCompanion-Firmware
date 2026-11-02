#include <TFT_eSPI.h>
#include "USB.h"
#include "USBHIDVendor.h"
#include "sprites.h"
#include <Preferences.h>

// Libellés par défaut, en autonome : l'appli envoie les siens dans la langue choisie (STATUT|Libellé).
// Police 4 sans accents.
#define TXT_AVAILABLE "Disponible"
#define TXT_AWAY "Absent"
#define TXT_CALL "En appel"
#define TXT_VIDEO "En visio"
#define TXT_BUSY "Occupe"
#define TXT_DND "Ne pas deranger"
#define TXT_UPDATE "Mise a jour"
#define TXT_BRB "Je reviens vite"

// Reçoit une ligne par statut (OFFLINE, AVAILABLE, AWAY, BRB, CALL, VIDEO, BUSY, DND, UPDATE) en HID USB,
// ou sur le port série USB pour tester à la main depuis un moniteur série, et anime le compagnon (sprites 32x32 de sprites/<compagnon>/*.png, agrandis x SCALE).

// Version en BCD (0x0100 = 1.0), à garder égale à CompanionLink.ExpectedFirmware de l'appli PC.
#define FW_VERSION 0x0100

enum State { OFFLINE, AVAILABLE, AWAY, BRB, CALL, VIDEO, OCCUPIED, DND, UPDATING };

struct Anim { const uint8_t* frames; uint8_t count; uint16_t ms; };
const uint8_t A_AVAILABLE[] = {F_IDLE, F_IDLE, F_IDLE, F_LOOK, F_LOOK, F_IDLE, F_BLINK, F_IDLE,
                               F_IDLE, F_SQUASH, F_JUMP, F_JUMP, F_SQUASH, F_IDLE};
const uint8_t A_AWAY[] = {F_AWAY1, F_AWAY2};
const uint8_t A_CALL[] = {F_CALL1, F_CALL2, F_CALL1, F_CALL2, F_CALL2, F_CALL1};
const uint8_t A_BUSY[] = {F_IDLE, F_IDLE, F_IDLE, F_IDLE, F_BLINK, F_IDLE};  // les yeux restent sur l'écran du portable
const uint8_t A_DND[] = {F_BACK};
const uint8_t A_UPDATE[] = {F_IDLE, F_IDLE, F_LOOK, F_IDLE, F_BLINK, F_IDLE, F_SQUASH, F_IDLE};
const uint8_t A_BRB[] = {F_LOOK};  // le compagnon passe juste une tête par le bord (drawPeek)
const uint8_t A_OFFLINE[] = {F_OFFLINE};

const int SCALE = 6;  // 32 x 6 = 192 px

USBHIDVendor hid;
USBCDC usbSerial;
TFT_eSPI tft;
TFT_eSprite spr(&tft);
State state = OFFLINE;
uint32_t lastMsg = 0;
String line;
String recvLabel;               // libellé localisé reçu de l'appli (vide = libellé par défaut)
State recvLabelState = OFFLINE;  // statut auquel recvLabel se rapporte
Companion companion = C_POUSSE;  // choisi dans l'appli (3e champ), mémorisé en flash (NVS) pour le prochain démarrage
Preferences prefs;
// Mains qui tapent en DND : index de palette contour, base, reflet de chaque compagnon.
const uint8_t HAND[C_COUNT][3] = {{1, 2, 4}, {1, 10, 12}, {1, 16, 5}, {1, 9, 5}, {1, 14, 9}};

State parse(const String& s) {
  if (s == "AVAILABLE") return AVAILABLE;
  if (s == "AWAY") return AWAY;
  if (s == "BRB") return BRB;
  if (s == "CALL") return CALL;
  if (s == "VIDEO") return VIDEO;
  if (s == "BUSY") return OCCUPIED;
  if (s == "DND") return DND;
  if (s == "UPDATE") return UPDATING;
  return OFFLINE;
}

void readLines(Stream& in) {
  while (in.available()) {
    char c = in.read();
    if (c == 0) continue;  // rapports HID complétés par des zéros
    if (c == '\n') {
      line.trim();
      // Format "STATUT", "STATUT|Libellé" ou "STATUT|Libellé|N" : texte localisé et numéro du compagnon.
      int bar = line.indexOf('|'), bar2 = bar < 0 ? -1 : line.indexOf('|', bar + 1);
      state = parse(bar < 0 ? line : line.substring(0, bar));
      recvLabel = bar < 0 ? "" : line.substring(bar + 1, bar2 < 0 ? line.length() : bar2);  // vide => libellé compilé
      if (bar2 >= 0) {
        int n = line.substring(bar2 + 1).toInt();
        if (n >= 0 && n < C_COUNT && n != companion) {
          companion = (Companion)n;
          prefs.putUChar("companion", n);  // écrit seulement au changement : la flash s'use
        }
      }
      recvLabelState = state;
      lastMsg = millis();
      line = "";
    } else if (line.length() < 40) {  // "AVAILABLE|Videochiamata|2" et autres libellés localisés
      line += c;
    }
  }
}

void readStatus() {
  readLines(hid);
  readLines(usbSerial);
  // Le PC renvoie toutes les 10 s : silence prolongé => PC éteint ou en veille.
  if (millis() - lastMsg > 35000) state = OFFLINE;
}

void drawFrame(Frame f, int ox, int oy, const uint16_t* pal) {
  const uint8_t* p = SPR_FRAMES[companion][f];
  for (int y = 0; y < SPR_SIZE; y++)
    for (int x = 0; x < SPR_SIZE; x++)
      if (uint8_t c = p[y * SPR_SIZE + x]) spr.fillRect(ox + x * SCALE, oy + y * SCALE, SCALE, SCALE, pal[c]);
}

// DND : feuille blanche en paysage avec STOP en rouge, scotchée dans le dos du compagnon (frame F_BACK), centrée en (cx, cy).
void drawStop(int cx, int cy) {
  const int fw = 70, fh = 46, x = cx - fw / 2, y = cy - fh / 2;
  spr.fillRect(x, y, fw, fh, tft.color24to16(0xB4B4BE));  // bord
  spr.fillRect(x + 1, y + 1, fw - 2, fh - 2, TFT_WHITE);
  spr.fillRect(x - 4, y - 3, 14, 6, TFT_LIGHTGREY);  // scotch aux coins du haut
  spr.fillRect(x + fw - 10, y - 3, 14, 6, TFT_LIGHTGREY);
  spr.setTextFont(1);
  spr.setTextSize(2);  // police 5x7 doublée : pixelisée comme les sprites
  spr.setTextColor(tft.color24to16(0xE81123));
  spr.setTextDatum(MC_DATUM);
  spr.drawString("STOP", cx + 1, cy + 1);
  spr.setTextSize(1);
}

// DND : vu de dos, le compagnon tape sur un portable posé devant lui. Mains de chaque côté qui frappent
// à tour de rôle (une frappe toutes les 130 ms), touche qui saute côté frappe. À dessiner après le compagnon.
void drawTyping(uint32_t t, int ox, int oy, const uint16_t* pal) {
  const uint8_t* c = HAND[companion];
  bool left = t / 130 % 2;
  uint32_t age = t % 130;
  for (int side = 0; side < 2; side++) {
    int x = ox + (side ? 25 : 7) * SCALE, y = oy + 27 * SCALE - ((side == 0) == left ? 0 : 5);  // main levée : 5 px
    spr.fillCircle(x, y, 10, pal[c[0]]);
    spr.fillCircle(x, y, 8, pal[c[1]]);
    spr.fillCircle(x - 3, y - 3, 3, pal[c[2]]);
  }
  int kx = ox + (left ? 4 : 28) * SCALE;
  spr.fillRect(kx - 3, oy + 25 * SCALE - age / 8, 6, 6, TFT_WHITE);
}

// OFFLINE : câble qui sort de derrière le compagnon (bas droit) et finit par une prise débranchée posée au sol.
// Coordonnées pour le sprite F_OFFLINE en (ox, oy).
void drawPlug(int ox, int oy) {
  const float x0 = ox + 25 * SCALE, y0 = oy + 26 * SCALE, x1 = ox + 25 * SCALE, y1 = oy + 36 * SCALE,
              x2 = ox + 28 * SCALE, y2 = oy + 33 * SCALE;  // courbe de Bézier : départ, creux, arrivée
  for (int i = 0; i <= 24; i++) {
    float k = i / 24.f, a = (1 - k) * (1 - k), b = 2 * k * (1 - k), c = k * k;
    spr.fillCircle(a * x0 + b * x1 + c * x2, a * y0 + b * y1 + c * y2, 3, TFT_BLACK);
  }
  int px = x2, py = y2;
  spr.fillRoundRect(px - 2, py - 9, 20, 18, 4, tft.color24to16(0x444444));
  spr.fillRect(px + 18, py - 6, 9, 4, TFT_LIGHTGREY);
  spr.fillRect(px + 18, py + 2, 9, 4, TFT_LIGHTGREY);
}

// BUSY : portable vu de dos devant le compagnon (seuls ses yeux dépassent), touches qui sautent du clavier
// pendant qu'il tape, tasse de café qui fume à gauche. À dessiner après le compagnon.
void drawLaptop(uint32_t t, int ox, int oy) {
  const int l = ox + 7 * SCALE, top = oy + 22 * SCALE, lw = 18 * SCALE, lh = 9 * SCALE, bottom = top + lh;
  const uint16_t pink = tft.color24to16(0xF4C6CE);
  for (int i = 0; i < 3; i++) {  // 3 dernières frappes, une toutes les 130 ms, à une colonne pseudo-aléatoire
    uint32_t j = t / 130 - i, age = t % 130 + i * 130;
    int sz = i == 0 ? 6 : 4;
    spr.fillRect(ox + SCALE * (10 + j * 7 % 13), top - 4 - age / 20, sz, sz, i == 0 ? TFT_WHITE : pink);
  }
  spr.fillRect(l, top, lw, lh, tft.color24to16(0x5A5A64));
  spr.fillRect(l + 2, top + 2, lw - 4, lh - 4, tft.color24to16(0xC8C8D2));
  spr.fillCircle(l + lw / 2, top + lh / 2, 6, TFT_WHITE);  // logo
  spr.fillRect(ox + 5 * SCALE, bottom, 22 * SCALE, 6, tft.color24to16(0x8C8C96));

  for (int r = 5; r <= 7; r++) spr.drawCircle(50, bottom - 12, r, TFT_WHITE);  // anse
  spr.fillRect(30, bottom - 24, 20, 24, TFT_WHITE);
  spr.fillRect(32, bottom - 22, 16, 4, tft.color24to16(0x6F4E37));
  for (int w = 0; w < 2; w++)  // 2 volutes de vapeur qui montent en ondulant
    for (int k = 0; k < 5; k++) {
      int y = bottom - 30 - (k * 6 + t / 40) % 30;
      spr.fillCircle(36 + w * 8 + (int)(3 * sinf((y + w * 10) / 5.f)), y, 2, pink);
    }
}

// UPDATE : pluie de 0 et de 1 qui tombe du haut de l'écran et disparaît derrière le compagnon (il « avale » le code).
// À dessiner avant le compagnon. 5 colonnes au-dessus de son corps, chacune à sa vitesse, 3 chiffres par colonne.
void drawCodeRain(uint32_t t, int ox, int oy) {
  spr.setTextFont(1);
  spr.setTextSize(2);  // police 5x7 doublée : 12x16 px, pixelisée comme les sprites
  spr.setTextDatum(TC_DATUM);
  const int fall = oy + 22 * SCALE + 40;  // course : du haut de l'écran jusqu'au milieu du corps
  for (int col = 0; col < 5; col++) {
    uint32_t period = 900 + col * 170, ph = t + col * 377;
    int head = (ph % period) * fall / period - 20;
    uint32_t cycle = ph / period;  // change les chiffres à chaque chute
    for (int k = 0; k < 3; k++) {
      spr.setTextColor(k == 0 ? tft.color24to16(0xB6FFB6) : k == 1 ? tft.color24to16(0x3CD86E) : tft.color24to16(0x1F7A3D));
      spr.drawString(((cycle + col * 3 + k) * 7 % 3) ? "1" : "0", ox + (8 + 4 * col) * SCALE, head - k * 18);
    }
  }
  spr.setTextSize(1);
}

// UPDATE : barre de progression en boucle sous le compagnon (la vraie progression n'est pas connue de l'écran).
void drawProgress(uint32_t t, int w, int y) {
  const int bw = 140, x = (w - bw) / 2, seg = 10;
  spr.drawRoundRect(x - 3, y - 3, bw + 6, 14, 4, TFT_WHITE);
  int filled = (t % 2400) * (bw / seg + 1) / 2400;  // segments allumés, 0 -> plein
  for (int i = 0; i < filled && i < bw / seg; i++) spr.fillRect(x + i * seg + 1, y, seg - 2, 8, tft.color24to16(0x3CD86E));
}

// BRB : sablier au centre, le sable du haut s'écoule en 4 s puis il repart plein. Centré en (cx, cy).
void drawHourglass(uint32_t t, int cx, int cy) {
  const int hh = 54, hw = 30, top = cy - hh, bot = cy + hh, gh = hh - 2;  // gh : hauteur d'un bulbe
  const uint16_t ink = tft.color24to16(0x1A1C2C), wood = tft.color24to16(0x965F32), sand = tft.color24to16(0xC8783C);
  float k = (t % 4000) / 4000.f;  // part du sable tombée
  spr.fillTriangle(cx - hw - 3, top, cx + hw + 3, top, cx, cy + 4, ink);  // contour du verre
  spr.fillTriangle(cx - hw - 3, bot, cx + hw + 3, bot, cx, cy - 4, ink);
  spr.fillTriangle(cx - hw, top + 2, cx + hw, top + 2, cx, cy, tft.color24to16(0xDCEBF5));
  spr.fillTriangle(cx - hw, bot - 2, cx + hw, bot - 2, cx, cy, tft.color24to16(0xDCEBF5));
  int s = (gh - 6) * sqrtf(1 - k);  // volume d'un triangle ∝ hauteur² : le niveau baisse vite puis lentement
  if (s > 0) spr.fillTriangle(cx - hw * s / gh, cy - s, cx + hw * s / gh, cy - s, cx, cy, sand);
  int m = gh * 0.7f * sqrtf(k);  // tas du bas, toujours plus étroit que le bulbe
  if (m > 0) spr.fillTriangle(cx - hw, bot - 2, cx + hw, bot - 2, cx, bot - 2 - m, sand);
  if (s > 0) spr.fillRect(cx - 1, cy, 2, bot - 2 - m - cy, sand);  // filet
  for (int y : {top - 10, bot}) {  // chapeau et socle en bois
    spr.fillRoundRect(cx - hw - 12, y - 1, 2 * hw + 24, 12, 3, ink);
    spr.fillRoundRect(cx - hw - 10, y + 1, 2 * hw + 20, 8, 2, wood);
  }
  spr.fillRect(cx - hw - 7, top, 4, 2 * hh, wood);  // montants
  spr.fillRect(cx + hw + 3, top, 4, 2 * hh, wood);
}

// BRB : position x du compagnon, qui sort par la gauche à l'arrivée du statut (700 ms) puis, toutes les 8 s,
// repasse une tête par le bord gauche pour surveiller le sablier. dt : temps depuis le début du statut.
int peekX(uint32_t dt, int center) {
  const int out = -SPR_SIZE * SCALE + 20, in = -70;  // caché / à moitié visible
  auto ease = [](float u) { return (1 - cosf(u * PI)) / 2; };
  if (dt < 700) return center + (out - center) * ease(dt / 700.f);
  uint32_t p = (dt - 700) % 8000;
  float e = p < 4500 ? 0 : p < 5100 ? ease((p - 4500) / 600.f) : p < 6800 ? 1 : p < 7400 ? 1 - ease((p - 6800) / 600.f) : 0;
  return out + (in - out) * e;
}

// Rend true quand l'animation doit rester fluide, pour que loop() redessine sans attendre.
bool draw(uint32_t t) {
  static State shown = OFFLINE;
  static uint32_t since = 0;  // début de l'état courant, pour le demi-tour de DND
  if (state != shown) { shown = state; since = t; }

  const int w = spr.width(), h = spr.height();
  Anim anim = {A_OFFLINE, 1, 1000};
  uint16_t pal[sizeof SPR_PALETTE / sizeof *SPR_PALETTE];
  memcpy(pal, SPR_PALETTE, sizeof pal);
  const char* label = "";  // hors ligne : pas de texte
  uint32_t bg = 0x8A8886;  // couleurs de présence Teams (orange au lieu du rouge pour « occupé »)

  // Corps de Pousse en violet Teams (index 2-4 : base, ombre, reflet) ; les autres compagnons n'utilisent pas ces index.
  pal[2] = tft.color24to16(0x6264A7); pal[3] = tft.color24to16(0x464775); pal[4] = tft.color24to16(0x9EA2FF);
  switch (state) {
    case AVAILABLE: anim = {A_AVAILABLE, sizeof A_AVAILABLE, 150}; label = TXT_AVAILABLE; bg = 0x6BB700; break;
    case AWAY: anim = {A_AWAY, sizeof A_AWAY, 800}; label = TXT_AWAY; bg = 0xFCD116; break;
    case CALL:
    case VIDEO:
      anim = {A_CALL, sizeof A_CALL, 180};
      label = state == VIDEO ? TXT_VIDEO : TXT_CALL;
      bg = 0xFF8C00;
      break;
    case OCCUPIED: anim = {A_BUSY, sizeof A_BUSY, 300}; label = TXT_BUSY; bg = 0xC4314B; break;
    case DND: anim = {A_DND, sizeof A_DND, 1000}; label = TXT_DND; bg = 0xC4314B; break;
    case BRB: anim = {A_BRB, sizeof A_BRB, 1000}; label = TXT_BRB; bg = 0xFCD116; break;
    case UPDATING: anim = {A_UPDATE, sizeof A_UPDATE, 250}; label = TXT_UPDATE; bg = 0x3A5078; break;
    default: break;
  }

  // Libellé envoyé par l'appli (langue choisie côté PC) ; sinon libellé compilé ci-dessus.
  if (recvLabel.length() && recvLabelState == state) label = recvLabel.c_str();

  uint32_t dt = t - since;
  Frame f = (Frame)anim.frames[(t / anim.ms) % anim.count];
  if (state == DND && dt < 250) f = F_LOOK;  // DND : regarde de côté puis tourne le dos
  if (state == BRB && dt >= 700 && (dt - 700) % 8000 / 150 == 39) f = F_BLINK;  // clin d'œil pendant qu'il regarde
  // compagnon centré au-dessus du texte (30 px)
  int ox = (w - SPR_SIZE * SCALE) / 2, oy = (h - SPR_SIZE * SCALE - 30) / 2 - (f == F_JUMP ? 2 * SCALE : 0);
  spr.fillSprite(tft.color24to16(bg));

  if (state == OFFLINE) drawPlug(ox, oy);
  if (state == UPDATING) drawCodeRain(t, ox, oy);
  if (state == BRB) {
    drawHourglass(t - since, w / 2, oy + 16 * SCALE);
    ox = peekX(dt, ox);
  }
  int bob = f == F_BACK && t % 130 < 40 ? 2 : 0;  // DND : le corps s'enfonce un peu à chaque frappe
  if (f == F_BACK) spr.fillRect(ox + 2 * SCALE, oy + 29 * SCALE, 28 * SCALE, 6, tft.color24to16(0x8C8C96));  // portable, derrière
  drawFrame(f, ox, oy + bob, pal);
  if (f == F_BACK) drawTyping(t, ox, oy + bob, pal);
  if (state == OCCUPIED) drawLaptop(t, ox, oy);
  if (f == F_BACK) drawStop(ox + 16 * SCALE, oy + 23 * SCALE + bob);

  if (state == VIDEO) {  // en retrait des coins arrondis de l'écran
    spr.fillRoundRect(w - 70, 20, 30, 22, 4, TFT_WHITE);
    spr.fillTriangle(w - 40, 31, w - 28, 23, w - 28, 39, TFT_WHITE);
    if (t % 1000 < 500) spr.fillCircle(38, 31, 7, TFT_WHITE);
  }

  if (state == UPDATING) drawProgress(t, w, oy + 32 * SCALE + 4);

  spr.setTextColor(state == UPDATING ? TFT_WHITE : TFT_BLACK);  // fond sombre
  spr.setTextDatum(BC_DATUM);
  if (state != OFFLINE) spr.drawString(label, w / 2, h - 8, 4);  // hors ligne : la prise débranchée suffit
  spr.pushSprite(0, 0);
  return state == UPDATING || state == BRB;  // pluie de code, sable et glissade doivent rester fluides
}

void setup() {
  // Waveshare 1.69 : SYS_EN haut garde la carte alimentée, buzzer passif à couper sinon il chauffe.
  pinMode(SYS_EN_PIN, OUTPUT);
  digitalWrite(SYS_EN_PIN, HIGH);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  prefs.begin("companion");
  uint8_t saved = prefs.getUChar("companion", C_POUSSE);
  if (saved < C_COUNT) companion = (Companion)saved;

  USB.productName("OpenCompanion");  // nom cherché par l'appli PC
  USB.firmwareVersion(FW_VERSION);   // lue par l'appli pour vérifier la version
  hid.begin();
  usbSerial.begin();
  USB.begin();
  tft.init();
  tft.fillScreen(TFT_BLACK);
  spr.setColorDepth(16);  // 240x280 en 16 bits = 134 Ko : couleurs fidèles (le 8 bits ternit les gris)
  spr.createSprite(tft.width(), tft.height());
}

void loop() {
  readStatus();
  if (!draw(millis())) delay(20);
}
