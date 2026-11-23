# Pointage par badge — câblage

| Composant | Broche | Arduino Uno |
|---|---|---|
| RC522 | SDA (SS) | 10 |
| RC522 | SCK | 13 |
| RC522 | MOSI | 11 |
| RC522 | MISO | 12 |
| RC522 | RST | 9 |
| RC522 | 3.3V / GND | **3.3V** / GND (jamais 5V) |
| LCD I2C | SDA / SCL | A4 / A5 |
| LCD I2C | VCC / GND | 5V / GND |
| Buzzer | + / − | 8 / GND |
| Servo (porte) | signal | 6 |
| Servo | + / − | 5V externe conseillé / GND commun |

Bibliothèques : `LiquidCrystal I2C`, `Servo`, `MFRC522`.
Si l'écran reste vide, l'adresse I2C peut être `0x3F` au lieu de `0x27`.

## Échange avec Qt (USB, 9600 bauds)

| Sens | Message | Effet |
|---|---|---|
| Arduino → Qt | `BADGE:04A1B2C3` | badge présenté |
| Qt → Arduino | `IN;Sarah;08:02` | arrivée, porte ouverte |
| Qt → Arduino | `OUT;Sarah;8h05` | départ, porte ouverte |
| Qt → Arduino | `WAIT;Sarah` | badge déjà lu à l'instant |
| Qt → Arduino | `NO;Badge inconnu` | refus, alarme |
| Qt → Arduino | `ERR` | base indisponible, refus |

## Simulation Wokwi

Mettre `SIMULATION` à `1`, coller le programme et `diagram.json` dans un projet Arduino Uno ; les boutons remplacent les badges.
Taper la réponse (par exemple `IN;Sarah;08:02`) dans le moniteur série dans les 3 secondes.
