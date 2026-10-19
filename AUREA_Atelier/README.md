# AUREA — Atelier (Employés + Matières + Production)

Application **C++17 / Qt 6** regroupant trois interfaces :

- **Gestion des employés** (tableau de bord, employés, affectation, pointage, statistiques)
- **Gestion des matières** (stock, fiche matière, réapprovisionnement, lots & QR codes, statistiques, historique des mouvements)
- **Gestion de la production** (tableau de bord, tâches, planning automatique, suivi de l'avancement, statistiques)

Les boutons de la colonne de gauche ouvrent le module choisi ; la rangée du haut en liste les pages.
Raccourcis : **Alt+1** (employés), **Alt+2** (matières), **Alt+3** (production). Le dernier module ouvert est mémorisé.
Les trois modules partagent le même thème sombre / or (Inter + Cormorant Garamond) ; chacun charge sa propre feuille de style.

## Compiler

- **Qt Creator** : ouvrir `CMakeLists.txt` (Qt 6.x, MinGW 64 bits ou MSVC) → *Build*. Cible : `AureaAtelier`.
  Alternative : `AureaAtelier.pro` (qmake). Modules Qt requis : Widgets, Sql, PrintSupport, Charts. Le projet compile aussi du C (bibliothèque `quirc`, lecture des QR codes).
- Ligne de commande : `cmake -S . -B build -G Ninja && cmake --build build`

## Oracle (ODBC)

Option `--oracle` : tous les modules utilisent Oracle (DSN `Source_Projet2A`, mot de passe dans la variable d'environnement
`AUREA_ORACLE_PASSWORD`) au lieu de SQLite. `--oracle-test` teste la connexion, `--oracle-test-modules` teste les trois modules.
Voir **`ORACLE_MIGRATION.md`** et `oracle/` (schéma SQL).

## Données

Deux bases SQLite dans le dossier de données de l'application (`employes.sqlite`, `stock.sqlite`).
Le module Production range sa table `tache` dans `employes.sqlite` : ses tâches sont affectées aux employés du module Employés
(bouton « Suggérer », employé indisponible refusé). Le numéro de commande d'une tâche se saisit librement (le module Commandes
n'est plus dans ce projet ; s'il est ajouté, la liste déroulante des vraies commandes réapparaît).
Options : `--data-dir <dossier>`, `--demo` (bases de démonstration séparées), `--render <dossier>` (captures d'écran).

## Organisation

| Dossier | Contenu |
|---|---|
| `main.cpp` | Démarrage, polices, bases de données, fenêtre principale |
| `shell/` | Fenêtre principale : barre du haut + zone de contenu (`ShellWindow`) |
| `employees/` | Module Employés (fenêtre, module, dépôt, PDF, thème, polices) |
| `materials/` | Module Matières (fenêtre, dépôt `InventoryRepository`, QR codes, PDF, `third_party/` : qrcodegen et quirc) |
| `production/` | Module Production (fenêtre, pages, règles métier `TaskStore`, graphiques) |
| `common/` | Icônes dessinées en code, dialecte SQL, connexion Oracle |
| `oracle/` | Scripts SQL du schéma Oracle (employés, production, matières) |
| `tests/` | Tests automatisés des modules Employés et Matières (`ctest`) |
