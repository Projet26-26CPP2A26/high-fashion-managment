# AUREA — Gestion de la production (C++ / Qt 6)

Module **Production** du chef d'atelier, dans le même style que le module Gestion des employés
(sidebar AUREA, cartes arrondies, thème sombre et or).

## Lancer
- **Qt Creator** : ouvrir `CMakeLists.txt` (ou `AureaProduction.pro`), choisir le kit *Qt 6 / MinGW 64 bits*, puis Ctrl+R.
- Option `--empty` : démarrer sans les tâches d'exemple. Option `--database chemin.sqlite` : choisir la base.
  (Qt Creator : Projets › Exécuter › Arguments en ligne de commande.)

## Les cinq pages (Ctrl+1 à Ctrl+5)
1. **Tableau de bord** : indicateurs, charge des 7 prochains jours, tâches prioritaires, échéances, étape à surveiller, alerte retards.
2. **Tâches** : CRUD complet (boutons **Modifier** et **Supprimer** sur chaque ligne et dans la barre d'outils), recherche (barre du haut),
   filtres état/priorité, tri par colonne, export PDF de la liste affichée.
3. **Planning automatique** : tâches non terminées classées par priorité puis date de livraison, avec calendrier.
4. **Suivi** : progression par commande et détection de l'étape qui ralentit l'atelier
   (retard = avancement attendu d'après les dates − avancement réel ; l'étape au retard moyen le plus élevé est signalée).
5. **Statistiques** : tâches par étape, répartition par priorité et par état, export PDF.

## Lien avec le module Gestion des employés
Les deux modules peuvent partager **la même base SQLite**. Si la table `employees` existe (créée par le module employés),
le formulaire de tâche propose les employés par leur nom, le bouton **Suggérer** choisit l'employé disponible de la bonne
spécialité qui a le moins de tâches en cours, et un employé indisponible est refusé et signalé en orange dans la liste.
Sans cette table, le formulaire demande simplement l'ID employé. Pour utiliser la base de l'autre module : ouvrir son
bouton Aide (il affiche « Base active : … ») et passer ce chemin avec `--database`.

## Fichiers
| Fichier | Rôle |
|---|---|
| `main.cpp` | Démarre Qt, charge polices, thème, base, fenêtre. |
| `mainwindow.h/.cpp` | Sidebar, barre du haut, navigation, raccourcis, aide. |
| `pages.h/.cpp` | Les cinq pages, tableau des tâches (modèle, tri/filtre, boutons de ligne) et formulaire. |
| `store.h/.cpp` | Données (SQLite) et règles : validation, planning, retard par étape. Sans interface. |
| `uikit.h/.cpp` | Icônes vectorielles et graphiques (courbe, barres). |
| `database.*`, `pdfexport.*` | Connexion SQLite et export PDF (repris du module employés). |
| `theme.qss`, `assets.qrc`, `assets/` | Style, logo et polices. |
| `documentation/` | Captures des cinq pages. |

Table SQLite : `tache(id, id_commande, etape, id_employe, date_debut, date_fin_prevue, priorite, avancement)`.
