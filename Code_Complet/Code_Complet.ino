/*
  Projet : Détection IR -> ouverture du portail + léger son
           Photorésistance -> effet de couleur ambiant fluide (bleu->vert->rouge)
  --------------------------------------------------------------------------------
  Matériel (NodeMCU ESP8266) :
    - Capteur optique/IR de proximité : VCC 3V, GND, OUT -> D2 (GPIO4)
    - Buzzer passif : + -> D1 (GPIO5), - -> GND
    - Photorésistance (diviseur de tension) :
        3.3V -> patte haute LDR -> [noeud -> A0] -> patte haute résistance -> GND
    - Bandeau LED RGB piloté par MOSFET IRLB8721PBF :
        D8 (GPIO15) -> R2 (220Ω) -> Gate Q1 -> Drain Q1 -> R (rouge)
        D7 (GPIO13) -> R3 (220Ω) -> Gate Q2 -> Drain Q2 -> G (vert)
        D6 (GPIO12) -> R4 (220Ω) -> Gate Q3 -> Drain Q3 -> B (bleu)

  Comportement :
    1. En permanence, le bandeau affiche un dégradé fluide bleu (sombre)
       -> vert -> rouge (clair) piloté par la photorésistance, avec un
       lissage exponentiel pour que ça glisse en douceur au lieu de sauter.
    2. Dès que le capteur IR détecte quelque chose
         -> l'animation portail se joue UNE FOIS (bips + ouverture +
            ~2s de respiration + fermeture, soit ~5s au total)
         -> puis retour immédiat à l'effet lumière
       Pour redéclencher, le capteur doit d'abord repasser au repos.

  Anti-rebond : le capteur IR est filtré (DEBOUNCE_MS) pour éviter les
  fausses détections répétées si son signal est instable.
*/

// ---------- BROCHES ----------
const uint8_t PIN_IR     = D2;   // capteur IR/optique (numérique)
const uint8_t PIN_BUZZER = D3;   // buzzer passif
const uint8_t PIN_LDR    = A0;   // photorésistance (analogique)
const uint8_t PIN_R      = D8;   // bandeau - rouge (via MOSFET)
const uint8_t PIN_G      = D7;   // bandeau - vert  (via MOSFET)
const uint8_t PIN_B      = D6;   // bandeau - bleu  (via MOSFET)

// ---------- RÉGLAGES ----------
#define IR_ACTIVE         LOW   // le capteur sort 0 quand il DÉTECTE (vérifié : main devant = IR brut 0)
const unsigned long DEBOUNCE_MS = 200;   // stabilité exigée avant de valider un changement d'état IR
const unsigned long HOLD_MS = 1500;      // une détection reste "vraie" au moins ce temps, même si le capteur décroche

// Plage mesurée de la photorésistance (à ajuster si besoin via le Moniteur Série)
const int LDR_MIN = 57;    // valeur dans le noir
const int LDR_MAX = 400;   // valeur en pleine lumière
const float LISSAGE = 0.05; // 0-1, plus petit = fondu plus lent/doux

// Exposant gamma : < 1 = LEDs plus "fortes" à intensité PWM égale
const float GAMMA_EXP = 0.35;
uint8_t gamma8[256];

// Couleur de base du portail : vert-cyan façon "portal gun"
const uint8_t PORTAL_R = 20;
const uint8_t PORTAL_G = 255;
const uint8_t PORTAL_B = 90;

// ---------- VARIABLES D'ÉTAT ----------
bool armeIR = true;   // le portail ne peut se redéclencher qu'une fois l'objet reparti

bool derniereLectureIR = false;
bool etatIRStable = false;
unsigned long dernierChangementIR = 0;

float lueLissee = 0;
uint8_t curR = 0, curG = 0, curB = 0;   // dernière couleur affichée (pour les fondus)

// ---------- BANDEAU ----------
void buildGammaTable() {
  for (int i = 0; i < 256; i++) {
    gamma8[i] = (uint8_t)(pow((float)i / 255.0, GAMMA_EXP) * 255.0 + 0.5);
  }
}

void setColor(uint8_t r, uint8_t g, uint8_t b) {
  curR = r; curG = g; curB = b;
  analogWrite(PIN_R, gamma8[r]);
  analogWrite(PIN_G, gamma8[g]);
  analogWrite(PIN_B, gamma8[b]);
}

// --- Ouverture du portail : fondu depuis la couleur actuelle + scintillement ---
void openPortal(uint16_t durationMs = 1200) {
  uint8_t startR = curR, startG = curG, startB = curB;
  uint16_t steps = durationMs / 15;
  for (uint16_t i = 0; i <= steps; i++) {
    float t = (float)i / steps;
    int flicker = random(-25, 25);
    uint8_t r = constrain((int)(startR + (PORTAL_R - startR) * t) + flicker, 0, 255);
    uint8_t g = constrain((int)(startG + (PORTAL_G - startG) * t) + flicker, 0, 255);
    uint8_t b = constrain((int)(startB + (PORTAL_B - startB) * t) + flicker, 0, 255);
    setColor(r, g, b);
    delay(15);
  }
}

// --- Portail stable : respiration douce + scintillement (à appeler en boucle) ---
void portalIdle() {
  float breathe = (sin(millis() / 400.0) + 1.0) / 2.0;
  int flicker = random(-10, 15);
  uint8_t r = constrain((int)(PORTAL_R * (0.85 + 0.15 * breathe)) + flicker, 0, 255);
  uint8_t g = constrain((int)(PORTAL_G * (0.85 + 0.15 * breathe)) + flicker, 0, 255);
  uint8_t b = constrain((int)(PORTAL_B * (0.85 + 0.15 * breathe)) + flicker, 0, 255);
  setColor(r, g, b);
}

// --- Fermeture du portail : descente en intensité ---
void closePortal(uint16_t durationMs = 800) {
  uint16_t steps = durationMs / 15;
  for (uint16_t i = 0; i <= steps; i++) {
    float t = 1.0 - (float)i / steps;
    int flicker = random(-15, 15);
    uint8_t r = constrain((int)(PORTAL_R * t) + flicker, 0, 255);
    uint8_t g = constrain((int)(PORTAL_G * t) + flicker, 0, 255);
    uint8_t b = constrain((int)(PORTAL_B * t) + flicker, 0, 255);
    setColor(r, g, b);
    delay(15);
  }
  setColor(0, 0, 0);
}

// --- Bips d'activation quand le portail s'allume (buzzer PASSIF : tone()) ---
// Série de bips courts sur ~1s, pas un son continu.
// Le bandeau est coupé pendant le son (tone() perturbe le PWM logiciel de
// l'ESP8266, ce qui causerait un léger scintillement sinon), puis remis
// exactement à la couleur qu'il avait avant.
void sonPortail() {
  uint8_t avantR = curR, avantG = curG, avantB = curB;
  setColor(0, 0, 0);   // coupe le bandeau pendant le son

  int frequences[] = {600, 800, 1000, 1200};
  for (int i = 0; i < 4; i++) {
    tone(PIN_BUZZER, frequences[i], 150);
    delay(250);
  }
  noTone(PIN_BUZZER);

  setColor(avantR, avantG, avantB);   // on remet la couleur d'avant
}

// --- Scintillement façon flamme/bougie (à appeler en boucle pendant l'effet) ---
void candleFlicker() {
  uint8_t r = 200 + random(0, 56);   // 200-255
  uint8_t g = 60 + random(0, 80);    // 60-140
  uint8_t b = 0;
  setColor(r, g, b);
  delay(random(30, 100));   // vitesse irrégulière, façon vraie flamme
}

void setup() {
  pinMode(PIN_IR, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);

  analogWriteRange(255);
  analogWriteFreq(1000);
  buildGammaTable();

  lueLissee = analogRead(PIN_LDR);   // démarre déjà calé sur la lecture actuelle
  setColor(0, 0, 0);
  randomSeed(analogRead(A0));

  Serial.begin(9600);
}

void loop() {
  gererCapteurIR();   // si détection -> joue l'animation portail (une fois)
  effetLumiere();     // le reste du temps : couleurs selon la photorésistance
  delay(15);
}

// ----- Lecture anti-rebond du capteur IR, avec maintien anti-creux -----
// Le capteur peut "décrocher" brièvement alors que l'objet est toujours là :
// une fois une détection validée, on la considère vraie pendant HOLD_MS même
// si le signal retombe entre-temps.
bool lireIRDebounce() {
  static unsigned long derniereDetection = 0;

  bool lecture = (digitalRead(PIN_IR) == IR_ACTIVE);
  if (lecture != derniereLectureIR) {
    dernierChangementIR = millis();
    derniereLectureIR = lecture;
  }
  if ((millis() - dernierChangementIR) > DEBOUNCE_MS) {
    etatIRStable = derniereLectureIR;
  }

  if (etatIRStable) {
    derniereDetection = millis();
  }

  static unsigned long dernierLogIR = 0;
  if (millis() - dernierLogIR > 500) {
    dernierLogIR = millis();
    Serial.print("IR brut=");
    Serial.println(digitalRead(PIN_IR));
  }

  // détection considérée active tant qu'on est dans la fenêtre de maintien
  return (millis() - derniereDetection) < HOLD_MS;
}

// ----- Capteur IR : joue l'animation portail une fois à la détection -----
void gererCapteurIR() {
  bool detecte = lireIRDebounce();

  if (detecte && armeIR) {
    armeIR = false;
    animationPortail();
  }
  if (!detecte) {
    armeIR = true;   // le capteur est repassé au repos, on peut redéclencher
  }
}

// ----- Animation portail complète : son + ouverture + maintien + fermeture -----
void animationPortail() {
  sonPortail();
  openPortal();

  unsigned long debut = millis();
  while (millis() - debut < 2000) {   // portail bien visible 2s
    portalIdle();
    delay(15);
  }

  closePortal();
}

// Convertit une teinte (0-359°) en RGB saturé à fond
void hueToRGB(uint16_t hue, uint8_t &r, uint8_t &g, uint8_t &b) {
  uint8_t region = hue / 60;
  uint8_t remainder = (hue % 60) * 255 / 60;

  switch (region) {
    case 0: r = 255; g = remainder; b = 0; break;
    case 1: r = 255 - remainder; g = 255; b = 0; break;
    case 2: r = 0; g = 255; b = remainder; break;
    case 3: r = 0; g = 255 - remainder; b = 255; break;
    case 4: r = remainder; g = 0; b = 255; break;
    default: r = 255; g = 0; b = 255 - remainder; break;
  }
}

// ----- Effet lumière ambiant : dégradé fluide piloté par la photorésistance -----
// Parcourt tout le spectre : violet (sombre) -> bleu -> cyan -> vert -> jaune
// -> orange -> rouge (clair), au lieu de seulement 3 couleurs.
void effetLumiere() {
  int brut = analogRead(PIN_LDR);
  lueLissee += (brut - lueLissee) * LISSAGE;   // lissage exponentiel, évite les à-coups

  int val = constrain((int)lueLissee, LDR_MIN, LDR_MAX);
  float t = (float)(val - LDR_MIN) / (LDR_MAX - LDR_MIN);   // 0 (sombre) -> 1 (clair)

  // teinte 280° (violet) dans le noir -> 0° (rouge) en pleine lumière
  uint16_t hue = (uint16_t)(280 - 280 * t);

  uint8_t r, g, b;
  hueToRGB(hue, r, g, b);
  setColor(r, g, b);

  static unsigned long dernierLog = 0;
  if (millis() - dernierLog > 300) {
    dernierLog = millis();
    Serial.print("LDR brut=");
    Serial.print(brut);
    Serial.print("  lisse=");
    Serial.print(lueLissee);
    Serial.print("  t=");
    Serial.print(t);
    Serial.print("  R=");
    Serial.print(r);
    Serial.print(" G=");
    Serial.print(g);
    Serial.print(" B=");
    Serial.println(b);
  }
}
