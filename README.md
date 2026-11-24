# AUREA — Gestion des matières

Application **C++17 / Qt 6** : matières premières, ajout/modification, réapprovisionnement, lots & QR codes, statistiques, export PDF.
Base de données : **Oracle** (ODBC, pilote Qt `QODBC`) — même code que l'intégration finale *AUREA Atelier*.

## Lancer

Double-cliquer sur `Run.bat` (`Run.bat -SansWokwi` pour ne pas ouvrir le simulateur).

## Compiler

- **Qt Creator** : ouvrir `CMakeLists.txt` (Qt 6.x, kit MinGW 64 bits) → cible `AureaMatieres`. Alternative : `AureaMatieres.pro` (qmake).
- Modules Qt requis : Widgets, Sql, PrintSupport (+ Test pour `ctest`).
- Ligne de commande : `cmake -S . -B build && cmake --build build`

## Configurer Oracle

1. Dans SQL Developer, exécuter `oracle/01_schema.sql` **une seule fois** (si la table `materials` n'existe pas encore).
2. Créer une source ODBC 64 bits (DSN par défaut : `Source_Projet2A`).
3. Qt Creator → Projets → Exécuter → Environnement :

| Variable | Rôle |
|---|---|
| `AUREA_ORACLE_PASSWORD` | mot de passe Oracle (**obligatoire**) |
| `AUREA_ORACLE_DSN` | source ODBC (défaut : `Source_Projet2A`) |
| `AUREA_ORACLE_USER` | utilisateur (défaut : `Iyed`) |

## Base de données

Table : `materials`. Schéma : `oracle/01_schema.sql` ; `oracle/99_drop_all.sql` pour la supprimer (base de test uniquement).

## Organisation

| Dossier | Contenu |
|---|---|
| `Run.bat` | Lance tout : Oracle, compilation Qt, simulateur Wokwi, AUREA |
| `Crud/` | Fenêtre, classe `Material`, dépôt `InventoryRepository`, connexion Oracle |
| `Metier/` | Réapprovisionnement, QR code par lot, statistiques et PDF |
| `arduino/` | Classe `Arduino`, page de la borne, programme `.ino`, projet Wokwi |
| `Animations/` | Ouverture, fermeture, fils de soie, logo, QR, tableaux, graphique, liaison |
| `Animations/ClosingAnimation` | Module commun de l'équipe, repris du projet AUREA_Employes |
| `Extras/` | Messages animés, icônes, sons, script de lancement, bibliothèques QR (`libs/`) |
| `Resources/Assets/` | Thème, polices, logo, icônes des matières, sons |
| `oracle/` | Scripts SQL |
| `tests/` | Tests automatisés (`ctest`, sans base de données) |

Raccourcis : **Ctrl+N** nouvelle matière, **Ctrl+F** rechercher, **Ctrl+S** enregistrer le formulaire.
