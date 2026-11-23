# AUREA — Gestion des employés

Application **C++17 / Qt 6** : tableau de bord, employés, affectation intelligente, pointage, statistiques, export PDF.
Base de données : **Oracle** (ODBC / QODBC).

## Compiler

- **Qt Creator** : ouvrir `CMakeLists.txt` (Qt 6.x) → cible `AureaEmployes`. Alternative : `AureaEmployes.pro` (qmake).
- Modules Qt requis : Widgets, Sql, PrintSupport.
- Ligne de commande : `cmake -S . -B build && cmake --build build`

## Lancer

| Option | Effet |
|---|---|
| *(aucune)* | Utiliser Oracle (DSN `Source_Projet2A`, mot de passe dans `AUREA_ORACLE_PASSWORD`) |
| `--oracle` | Conservé pour compatibilité (Oracle est toujours utilisé) |
| `--oracle-test` | Tester la connexion Oracle |
| `--oracle-test-modules` | Tester le CRUD du module Employés sur Oracle (données d'essai supprimées) |

## Base de données

Table : `employees` (`oracle/01_schema.sql`, identique au schéma de l'équipe). Pointage, affectations et sessions : fichier local `activity.sqlite`.
Badges : `oracle/03_badges.sql` (tables `employee_badges`, `badge_scans`). Exécuter 01 sauf si le schéma commun est déjà installé.

## Connexion (comptes de démonstration)

| Identifiant | Mot de passe | Espace |
|---|---|---|
| `admin` | `123456` | Responsable RH : gestion, suivi en direct, revues |
| `employe` | `123456` | Espace du premier employé de la base |

Sessions enregistrées dans `activity.sqlite` (dossier local de l'application) : Oracle n'est pas modifié.

## Démonstration

Par défaut, les autres employés sont **animés** (données fictives : horaires, pauses, tâches, dépôts) dans `activity_demo.sqlite`.
Option `--live` : sans animation, données réelles dans `activity.sqlite`. Oracle n'est jamais modifié par la démonstration.

## Organisation

| Dossier | Contenu |
|---|---|
| `main.cpp` | Démarrage, choix de l'espace |
| `connexion/` | Connexion Oracle (singleton) et login |
| `crud/` | Classe `Employee`, `EmployeeRepository` (ajouter, afficher, modifier, supprimer), interface CRUD, fenêtre principale |
| `metiers/` | Affectation intelligente, pointage et heures sup., statistiques, PDF |
| `arduino/` | Badge RFID : service Oracle, page Qt, carte virtuelle, programme Arduino, câblage |
| `suivi/` | Espace employé, tableau de bord RH, tâches et revues |
| `oracle/` | Scripts SQL |
| `extras/` | Animations, thème, icônes, démonstration, tests Oracle |

Raccourcis : **Ctrl+1 à Ctrl+4** pour changer de page ; double-clic sur une fiche pour la modifier.
