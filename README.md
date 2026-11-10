# AUREA — Gestion des matières

Module « Gestion des matières » extrait du projet AUREA Atelier (Qt 6 Widgets, C++17) :
inventaire, alertes de réapprovisionnement, lots & QR codes, statistiques, exports PDF.

## Base de données

| Mode | Lancement | Stockage |
|---|---|---|
| Oracle (ODBC, défaut) | `AureaMatieres` | table `materials` du schéma `IYED`, créées par `oracle/01_schema.sql` |
| SQLite (hors ligne) | `AureaMatieres --sqlite` | `stock.sqlite` dans le dossier de données (`--data-dir`), schéma créé automatiquement |

Oracle (atelier 2 — Connexion QT/Oracle, Oracle XE 21c) :
- utilisateur `Iyed` / `Esprit18`, créé dans la base enfichable `XEPDB1` (`grant connect, resource, dba`) ;
- source ODBC 64 bits `Source_Projet2A`, pilote `Oracle in OraDB21Home1`, TNS Service Name `XEPDB1` ;
- SQL Developer : nouvelle connexion `Iyed` / `Esprit18`, hôte `localhost`, port `1521`, nom de service `XEPDB1`.

Valeurs surchargées si besoin par `AUREA_ORACLE_DSN`, `AUREA_ORACLE_USER`, `AUREA_ORACLE_PASSWORD`.
`oracle/02_views.sql` crée 3 vues (une par page) visibles dans SQL Developer sous « Vues » :
`v_matieres`, `v_alertes`, `v_statistiques`.
`AureaMatieres --oracle-test` vérifie la connexion. `oracle/99_drop_all.sql` supprime la table (irréversible).

## Compiler

CMake : `find_package(Qt6 Widgets Sql PrintSupport)` — cible `AureaMatieres` (+ `material_tests` avec `-DBUILD_TESTING=ON`).
Qt Creator / qmake : ouvrir `AureaMatieres.pro` (kit Qt 6 MinGW **64 bits**). Le pilote `QODBC` doit être présent.

## Options

`--data-dir <dossier>`, `--demo`, `--sqlite`, `--oracle-test`.

## QR codes

Le QR d'un lot contient un lien `https://itty.bitty.site/#Lot-<lot>/<page>` : ce site gratuit (sans compte)
affiche la petite page HTML écrite dans le lien (compressée avec `qCompress`, en base64). Rien à héberger.
L'application relit le lot dans le titre `Lot-<lot>`.
Les images QR sont créées par l'API gratuite `api.qrserver.com` et lues par `zxing.org` : **internet est nécessaire** sur le PC
(module Qt Network, aucune clé). Dans l'application, on peut aussi taper le lot (`SOI-2601` ou `AUREA:LOT:SOI-2601`).
