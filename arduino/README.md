# Borne de stock Arduino (module Matières)

**Scénario.** Le magasinier tape l'ID d'une matière au clavier. Qt cherche la matière dans la table Oracle `MATERIALS`, enregistre l'entrée ou la sortie (`UPDATE`), puis renvoie le nom et le nouveau stock à l'écran LCD. Si le stock passe sous le seuil, le buzzer sonne trois fois et l'application affiche l'alerte de réapprovisionnement.

Clavier → Arduino → USB série (9600 bauds) → Qt → Oracle → Qt → Arduino → LCD + buzzer

## Composants et câblage (Arduino Uno)

| Composant | Broches |
|---|---|
| Clavier 4x4, lignes 1 à 4 | D9, D8, D7, D6 |
| Clavier 4x4, colonnes 1 à 4 | D5, D4, D3, D2 |
| LCD 16x2 I2C | VCC → 5V, GND → GND, SDA → A4, SCL → A5 |
| Buzzer passif | + → D10, - → GND |

Maquette : boîte en carton recyclé, l'écran et le clavier en façade, l'Arduino à l'intérieur.

## Utilisation

1. Téléverser `borne_stock/borne_stock.ino` avec l'IDE Arduino (bibliothèque *LiquidCrystal I2C*).
2. **Fermer le moniteur série** de l'IDE, puis lancer AUREA : « Arduino connecté » apparaît en bas à droite.
3. Consulter : `ID` puis `C`. Entrée : `ID` `A` quantité `#`. Sortie : `ID` `B` quantité `#`. Annuler : `*` ou `D`.

## Messages

| Arduino → Qt | Qt → Arduino |
|---|---|
| `C3` consulter, `E3:10` entrée, `S3:15` sortie | `ligne1\|ligne2\|état` (0 normal, 1 sous le seuil, 2 erreur) |

## Simuler sans matériel (Wokwi)

Sur <https://wokwi.com>, nouveau projet **Arduino Uno** : coller `borne_stock.ino` dans `sketch.ino`, `wokwi/diagram.json` dans `diagram.json`, et ajouter la bibliothèque *LiquidCrystal I2C*. Le simulateur ne voit pas Qt : dans le moniteur série, jouer le rôle de Qt en tapant par exemple `Fil de soie|Stock: 3 bobine|1`, moins de 5 secondes après la demande.

## Simulation reliée à Oracle (Wokwi for VS Code)

1. Installer VS Code et l'extension **Wokwi Simulator**, puis `F1` → *Wokwi: Request a New License*.
2. Ouvrir le dossier `arduino/wokwi` dans VS Code, puis `F1` → *Wokwi: Start Simulator*. Garder l'onglet de simulation visible.
3. Lancer AUREA (sans carte branchée) : « Arduino simulé connecté (Wokwi) » apparaît en bas à droite. Qt répond alors tout seul et met à jour Oracle.

Le port série simulé est partagé sur ce PC par `rfc2217ServerPort = 47110` (`wokwi.toml`).
