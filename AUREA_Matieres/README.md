# AUREA — Gestion des matières

Module « Gestion des matières » extrait du projet AUREA Atelier (Qt 6 Widgets, C++17) :
inventaire, alertes de réapprovisionnement, lots & QR codes, statistiques, historique des mouvements, exports PDF.

## Base de données

| Mode | Lancement | Stockage |
|---|---|---|
| SQLite (défaut) | `AureaMatieres` | `stock.sqlite` dans le dossier de données (`--data-dir`), schéma créé automatiquement |
| Oracle (ODBC) | `AureaMatieres --oracle` | tables `materials` et `movements`, créées par `oracle/01_schema.sql` |

Oracle : exécuter `oracle/01_schema.sql` connecté en `HSAN`, puis définir les variables d'environnement
`AUREA_ORACLE_PASSWORD` (obligatoire), `AUREA_ORACLE_DSN` (défaut `Source_Projet2A`) et `AUREA_ORACLE_USER` (défaut `hsan`).
`AureaMatieres --oracle-test` vérifie la connexion. `oracle/99_drop_all.sql` supprime les deux tables (irréversible).

## Compiler

CMake : `find_package(Qt6 Widgets Sql PrintSupport)` — cible `AureaMatieres` (+ `material_tests` avec `-DBUILD_TESTING=ON`).
Qt Creator / qmake : ouvrir `AureaMatieres.pro`. Le pilote `QODBC` doit être présent pour le mode Oracle.

## Options

`--data-dir <dossier>`, `--demo`, `--render <dossier>` (captures d'écran), `--oracle`, `--oracle-test`.
