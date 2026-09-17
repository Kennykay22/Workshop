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
    1. Par défaut, le bandeau affiche un dégradé fluide bleu (sombre)
       -> vert -> rouge (clair) piloté par la photorésistance, avec un
       lissage exponentiel pour que ça glisse en douceur au lieu de sauter.
    2. Le capteur IR détecte un objet proche
         -> le portail prend le dessus (effet vert/cyan) + léger bip
         -> respiration/scintillement tant que l'objet reste détecté
         -> à son départ : fermeture, puis retour à l'effet lumière ambiant

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
#define IR_ACTIVE         HIGH  // niveau du capteur IR quand il DÉTECTE (remets LOW si ça ne corrige pas le souci)
const unsigned long DEBOUNCE_MS = 60;  // stabilité exigée avant de valider un changement d'état IR

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
bool portalActive = false;

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
void sonPortail() {
  int frequences[] = {600, 800, 1000, 1200};
  for (int i = 0; i < 4; i++) {
    tone(PIN_BUZZER, frequences[i], 150);
    delay(250);
  }
  noTone(PIN_BUZZER);
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
  gererCapteurIR();

  if (portalActive) {
    portalIdle();
  } else {
    effetLumiere();   // dégradé bleu->vert->rouge piloté par la photorésistance
  }

  delay(15);
}

// ----- Lecture anti-rebond du capteur IR -----
bool lireIRDebounce() {
  bool lecture = (digitalRead(PIN_IR) == IR_ACTIVE);
  if (lecture != derniereLectureIR) {
    dernierChangementIR = millis();
    derniereLectureIR = lecture;
  }
  if ((millis() - dernierChangementIR) > DEBOUNCE_MS) {
    etatIRStable = derniereLectureIR;
  }

  static unsigned long dernierLogIR = 0;
  if (millis() - dernierLogIR > 500) {
    dernierLogIR = millis();
    Serial.print("IR brut=");
    Serial.println(digitalRead(PIN_IR));
  }

  return etatIRStable;
}

// ----- Capteur IR : ouvre/ferme le portail + léger son -----
void gererCapteurIR() {
  bool detecte = lireIRDebounce();
  static bool dejaJoue = false;

  if (detecte && !dejaJoue) {
    dejaJoue = true;
    portalActive = true;
    sonPortail();
    openPortal();
  }
  if (!detecte && dejaJoue) {
    dejaJoue = false;
    portalActive = false;
    closePortal();
  }
}

// ----- Effet lumière ambiant : dégradé fluide piloté par la photorésistance -----
void effetLumiere() {
  int brut = analogRead(PIN_LDR);
  lueLissee += (brut - lueLissee) * LISSAGE;   // lissage exponentiel, évite les à-coups

  int val = constrain((int)lueLissee, LDR_MIN, LDR_MAX);
  float t = (float)(val - LDR_MIN) / (LDR_MAX - LDR_MIN);   // 0 (sombre) -> 1 (clair)

  uint8_t r, g, b;
  if (t <= 0.5) {
    float t2 = t / 0.5;               // bleu -> vert
    r = 0;
    g = (uint8_t)(255 * t2);
    b = (uint8_t)(255 * (1 - t2));
  } else {
    float t2 = (t - 0.5) / 0.5;       // vert -> rouge
    r = (uint8_t)(255 * t2);
    g = (uint8_t)(255 * (1 - t2));
    b = 0;
  }
  setColor(r, g, b);

  static unsigned long dernierLog = 0;
  if (millis() - dernierLog > 300) {
    dernierLog = millis();
    Serial.print("LDR brut=");
    Serial.print(brut);
    Serial.print("  lisse=");
    Serial.println(lueLissee);
  }
}
