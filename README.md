# Portail lumineux avec détection IR (ESP8266)

Ce projet repose sur une carte **NodeMCU ESP8266**. Il pilote un bandeau LED RGB qui réagit à son environnement : la couleur suit la luminosité ambiante, et lorsqu'un capteur détecte un objet à proximité, un effet « portail » se déclenche avec un signal sonore. L'inspiration visuelle vient du *portal gun* (couleurs vert-cyan).

## Fonctionnement

Le programme alterne entre deux modes :

- **Mode ambiance** : le bandeau affiche un dégradé fluide qui suit la lumière de la pièce — bleu dans l'obscurité, puis vert, puis rouge en pleine lumière. Les transitions sont lissées pour éviter les à-coups.
- **Mode portail** : dès qu'un objet est détecté, l'effet ambiance laisse place à un fondu vers un vert-cyan lumineux, accompagné d'une série de bips. Tant que l'objet reste présent, la lumière « respire » avec un léger scintillement. À son départ, le portail se referme et le mode ambiance reprend.

## Matériel et branchements

| Composant | Broche |
|-----------|--------|
| Capteur IR de proximité (OUT) | D2 |
| Buzzer passif (+) | D3 |
| Photorésistance (LDR) | A0 |
| Bandeau RGB — rouge (via MOSFET) | D8 |
| Bandeau RGB — vert (via MOSFET) | D7 |
| Bandeau RGB — bleu (via MOSFET) | D6 |

La photorésistance est montée en **diviseur de tension** (`3.3V → LDR → A0 → résistance → GND`). Chaque couleur du bandeau est commandée par un **MOSFET IRLB8721PBF** (avec une résistance de 220Ω sur la gate), car le bandeau consomme trop de courant pour être alimenté directement par la carte.

## Les points clés du code

- **Correction gamma** : une table (`buildGammaTable`) recalcule les valeurs PWM pour que les fondus paraissent naturels à l'œil, plutôt que linéaires.
- **Lissage de la lumière** : la valeur de la photorésistance est adoucie progressivement (`LISSAGE`) au lieu d'être utilisée brute, ce qui évite les sauts de couleur.
- **Anti-rebond du capteur IR** : un changement d'état n'est validé que si le signal reste stable pendant `DEBOUNCE_MS`, ce qui empêche les fausses détections.
- **Effets portail** : `openPortal()` (fondu d'ouverture), `portalIdle()` (respiration en continu), `closePortal()` (fermeture) et `sonPortail()` (bips via `tone()`, car le buzzer est passif).

## Réglages ajustables

Plusieurs constantes en haut du fichier permettent d'adapter le comportement :

- `IR_ACTIVE` — niveau du capteur lors d'une détection (`HIGH` ou `LOW`)
- `LDR_MIN` / `LDR_MAX` — plage de la photorésistance (à relever via le Moniteur Série)
- `LISSAGE` — vitesse du fondu de l'ambiance
- `GAMMA_EXP` — intensité de la correction gamma
- `PORTAL_R / G / B` — couleur du portail

## Utilisation

1. Réaliser les branchements décrits ci-dessus.
2. Ouvrir le code dans l'IDE Arduino avec le support **ESP8266** installé.
3. Sélectionner la carte **NodeMCU** et le bon port.
4. Téléverser le programme.
5. Ouvrir le **Moniteur Série à 9600 bauds** pour observer les valeurs et ajuster les seuils.

Le bandeau s'anime alors automatiquement, et le portail se déclenche au passage d'un objet devant le capteur. 🎉
