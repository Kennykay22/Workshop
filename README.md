# Portail RickLab™ — Détecteur IR + Ambiance lumineuse (ESP8266)

Projet réalisé dans le cadre du Workshop national RickLab™ (EPSI, Bachelor 2, session Septembre 2026). Un portail lumineux inspiré de l'univers Rick et Morty : un bandeau LED RGB piloté par une carte NodeMCU ESP8266, qui réagit en continu à son environnement et se déclenche au passage d'une personne.

## Fonctionnement

Deux comportements cohabitent :

- **Mode ambiance** (actif en permanence) : la couleur du bandeau varie en fonction de la luminosité ambiante mesurée par une photorésistance (LDR) — violet/bleu dans l'obscurité, puis cyan, vert, jaune, jusqu'au rouge en pleine lumière. La lecture est lissée pour éviter les sauts de couleur.
- **Mode portail** (déclenché par le capteur infrarouge) : dès qu'une présence est détectée, l'ambiance est interrompue pour jouer une séquence complète — un signal sonore (buzzer), un fondu vers un vert-cyan caractéristique du portail, un effet de "respiration" tant que la présence est maintenue, puis un fondu vers le noir à la fermeture. Le mode ambiance reprend ensuite automatiquement.

## Matériel et branchements

| Composant | Broche ESP8266 | Détail |
|---|---|---|
| Capteur infrarouge MH-B | D2 | Sortie active à l'état bas (0 = détection) |
| Buzzer passif | D3 | Piloté via `tone()` |
| Photorésistance (LDR) | A0 | Montée en pont diviseur de tension avec une résistance fixe de 10 kΩ |
| MOSFET canal Rouge (Q1) | D8 → grille | IRLB8721, résistance de grille 220 Ω + pull-down 10 kΩ |
| MOSFET canal Vert (Q2) | D7 → grille | Même montage |
| MOSFET canal Bleu (Q3) | D6 → grille | Même montage |
| Bandeau LED RGB | — | Anode commune +5V (alimentation externe, indépendante de l'ESP8266) ; cathodes pilotées par les MOSFET |

Chaque canal de couleur est commuté côté masse (low-side switching), ce qui permet à un GPIO 3,3V de piloter une charge alimentée en 5V sans interface supplémentaire.

## Points clés du code

- **Correction gamma** (`construireTableGamma`) — compense la perception non linéaire de l'œil pour des dégradés PWM homogènes.
- **Lissage de la lecture LDR** (constante `LISSAGE`) — filtre exponentiel qui évite les sauts de couleur dus au bruit du capteur.
- **Anti-rebond IR** (`ANTI_REBOND_MS`, 200 ms) — un changement d'état du capteur n'est validé qu'après une durée de stabilité.
- **Maintien de détection** (`MAINTIEN_MS`, 1500 ms) — une présence reste considérée comme détectée jusqu'à 1,5 s après la dernière lecture stable, pour éviter que l'animation ne redémarre en boucle si le faisceau est momentanément coupé.
- **Séquence d'animation du portail** — `sonPortail()` (bips ascendants), `ouvrirPortail()` (fondu d'ouverture avec scintillement), `portailStable()` (respiration sinusoïdale), `fermerPortail()` (fondu de fermeture).

## Réglages ajustables

Constantes en haut du fichier `.ino` :

- `PHOTORESISTANCE_MIN` / `PHOTORESISTANCE_MAX` — plage de calibration de la LDR (à relever via le Moniteur Série selon la luminosité de la pièce)
- `LISSAGE` — vitesse de réaction du mode ambiance
- `EXPOSANT_GAMMA` — intensité de la correction gamma
- `PORTAIL_R` / `PORTAIL_G` / `PORTAIL_B` — couleur du portail à l'ouverture
- `ANTI_REBOND_MS` / `MAINTIEN_MS` — réglages de la détection IR

## Utilisation

1. Réaliser les branchements décrits ci-dessus.
2. Ouvrir le code dans l'IDE Arduino avec le support ESP8266 installé.
3. Sélectionner la carte **NodeMCU 1.0 (ESP-12E Module)** et le bon port.
4. Téléverser le programme.
5. Ouvrir le Moniteur Série à 9600 bauds pour observer les valeurs brutes de la LDR et ajuster `PHOTORESISTANCE_MIN` / `PHOTORESISTANCE_MAX` si besoin.

## Structure du dépôt

```
├── code/
│   └── ir_buzzer_ldr_bandeau.ino
├── 3D/
│   ├── Rick Workshop base.f3d
│   ├── portail.f3d
│   └── Rick Workshop cache diffusant led.f3d
├── images/
│   ├── Rick Workshop v7.png
│   ├── portail.png
│   ├── socle et cache.png
│   └── Cache difusant.png
├── schemas/
│   └── schema_electronique.png
├── documentation/
│   └── Workshop2026-B2g5-dossier.pdf
└── README.md
```
