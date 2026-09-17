const uint8_t PIN_INFRAROUGE = D2;
const uint8_t PIN_BUZZER     = D3;
const uint8_t PIN_PHOTORESISTANCE = A0;
const uint8_t PIN_R = D8;
const uint8_t PIN_G = D7;
const uint8_t PIN_B = D6;

#define INFRAROUGE_ACTIF LOW
const unsigned long ANTI_REBOND_MS = 200;
const unsigned long MAINTIEN_MS = 1500;

const int PHOTORESISTANCE_MIN = 57;
const int PHOTORESISTANCE_MAX = 400;
const float LISSAGE = 0.05;

const float EXPOSANT_GAMMA = 0.35;
uint8_t tableGamma8[256];

const uint8_t PORTAIL_R = 20;
const uint8_t PORTAIL_G = 255;
const uint8_t PORTAIL_B = 90;

bool armeIR = true;

bool derniereLectureIR = false;
bool etatIRStable = false;
unsigned long dernierChangementIR = 0;

float lueLissee = 0;
uint8_t actuelR = 0, actuelG = 0, actuelB = 0;

void construireTableGamma() {
  for (int i = 0; i < 256; i++) {
    tableGamma8[i] = (uint8_t)(pow((float)i / 255.0, EXPOSANT_GAMMA) * 255.0 + 0.5);
  }
}

void definirCouleur(uint8_t r, uint8_t g, uint8_t b) {
  actuelR = r; actuelG = g; actuelB = b;
  analogWrite(PIN_R, tableGamma8[r]);
  analogWrite(PIN_G, tableGamma8[g]);
  analogWrite(PIN_B, tableGamma8[b]);
}

void ouvrirPortail(uint16_t dureeMs = 1200) {
  uint8_t debutR = actuelR, debutG = actuelG, debutB = actuelB;
  uint16_t etapes = dureeMs / 15;
  for (uint16_t i = 0; i <= etapes; i++) {
    float t = (float)i / etapes;
    int scintillement = random(-25, 25);
    uint8_t r = constrain((int)(debutR + (PORTAIL_R - debutR) * t) + scintillement, 0, 255);
    uint8_t g = constrain((int)(debutG + (PORTAIL_G - debutG) * t) + scintillement, 0, 255);
    uint8_t b = constrain((int)(debutB + (PORTAIL_B - debutB) * t) + scintillement, 0, 255);
    definirCouleur(r, g, b);
    delay(15);
  }
}

void portailStable() {
  float respiration = (sin(millis() / 400.0) + 1.0) / 2.0;
  int scintillement = random(-10, 15);
  uint8_t r = constrain((int)(PORTAIL_R * (0.85 + 0.15 * respiration)) + scintillement, 0, 255);
  uint8_t g = constrain((int)(PORTAIL_G * (0.85 + 0.15 * respiration)) + scintillement, 0, 255);
  uint8_t b = constrain((int)(PORTAIL_B * (0.85 + 0.15 * respiration)) + scintillement, 0, 255);
  definirCouleur(r, g, b);
}

void fermerPortail(uint16_t dureeMs = 800) {
  uint16_t etapes = dureeMs / 15;
  for (uint16_t i = 0; i <= etapes; i++) {
    float t = 1.0 - (float)i / etapes;
    int scintillement = random(-15, 15);
    uint8_t r = constrain((int)(PORTAIL_R * t) + scintillement, 0, 255);
    uint8_t g = constrain((int)(PORTAIL_G * t) + scintillement, 0, 255);
    uint8_t b = constrain((int)(PORTAIL_B * t) + scintillement, 0, 255);
    definirCouleur(r, g, b);
    delay(15);
  }
  definirCouleur(0, 0, 0);
}

void sonPortail() {
  uint8_t avantR = actuelR, avantG = actuelG, avantB = actuelB;
  definirCouleur(0, 0, 0);

  int frequences[] = {600, 800, 1000, 1200};
  for (int i = 0; i < 4; i++) {
    tone(PIN_BUZZER, frequences[i], 150);
    delay(250);
  }
  noTone(PIN_BUZZER);

  definirCouleur(avantR, avantG, avantB);
}

void scintillementBougie() {
  uint8_t r = 200 + random(0, 56);
  uint8_t g = 60 + random(0, 80);
  uint8_t b = 0;
  definirCouleur(r, g, b);
  delay(random(30, 100));
}

void setup() {
  pinMode(PIN_INFRAROUGE, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);

  analogWriteRange(255);
  analogWriteFreq(1000);
  construireTableGamma();

  lueLissee = analogRead(PIN_PHOTORESISTANCE);
  definirCouleur(0, 0, 0);
  randomSeed(analogRead(A0));

  Serial.begin(9600);
}

void loop() {
  gererCapteurIR();
  effetLumiere();
  delay(15);
}

bool lireIRAntiRebond() {
  static unsigned long derniereDetection = 0;

  bool lecture = (digitalRead(PIN_INFRAROUGE) == INFRAROUGE_ACTIF);
  if (lecture != derniereLectureIR) {
    dernierChangementIR = millis();
    derniereLectureIR = lecture;
  }
  if ((millis() - dernierChangementIR) > ANTI_REBOND_MS) {
    etatIRStable = derniereLectureIR;
  }

  if (etatIRStable) {
    derniereDetection = millis();
  }

  static unsigned long dernierLogIR = 0;
  if (millis() - dernierLogIR > 500) {
    dernierLogIR = millis();
    Serial.print("IR brut=");
    Serial.println(digitalRead(PIN_INFRAROUGE));
  }

  return (millis() - derniereDetection) < MAINTIEN_MS;
}

void gererCapteurIR() {
  bool detecte = lireIRAntiRebond();

  if (detecte && armeIR) {
    armeIR = false;
    animationPortail();
  }
  if (!detecte) {
    armeIR = true;
  }
}

void animationPortail() {
  sonPortail();
  ouvrirPortail();

  unsigned long debut = millis();
  while (millis() - debut < 2000) {
    portailStable();
    delay(15);
  }

  fermerPortail();
}

void teinteVersRGB(uint16_t teinte, uint8_t &r, uint8_t &g, uint8_t &b) {
  uint8_t region = teinte / 60;
  uint8_t reste = (teinte % 60) * 255 / 60;

  switch (region) {
    case 0: r = 255; g = reste; b = 0; break;
    case 1: r = 255 - reste; g = 255; b = 0; break;
    case 2: r = 0; g = 255; b = reste; break;
    case 3: r = 0; g = 255 - reste; b = 255; break;
    case 4: r = reste; g = 0; b = 255; break;
    default: r = 255; g = 0; b = 255 - reste; break;
  }
}

void effetLumiere() {
  int brut = analogRead(PIN_PHOTORESISTANCE);
  lueLissee += (brut - lueLissee) * LISSAGE;

  int valeur = constrain((int)lueLissee, PHOTORESISTANCE_MIN, PHOTORESISTANCE_MAX);
  float t = (float)(valeur - PHOTORESISTANCE_MIN) / (PHOTORESISTANCE_MAX - PHOTORESISTANCE_MIN);

  uint16_t teinte = (uint16_t)(280 - 280 * t);

  uint8_t r, g, b;
  teinteVersRGB(teinte, r, g, b);
  definirCouleur(r, g, b);

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
