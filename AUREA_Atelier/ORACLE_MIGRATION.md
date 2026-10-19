# AUREA — Connexion Oracle (ODBC) : audit, mise en place, suite de la migration

> **Note :** ce projet ne contient plus que les modules **Employés**, **Matières** et **Production** (les modules Clients et Commandes en ont été retirés). Les sections 1 à 6 (audit) ont été écrites pour la version à cinq modules ; les passages sur Clients et Commandes ne s'appliquent plus. La section 7 est à jour.


## 1. État actuel (honnête)

| Élément | État |
|---|---|
| Composant de connexion Oracle `OracleConnection` (QODBC, singleton de Meyers) | **Compilé et testé sur Oracle 11g XE** |
| Test de bout en bout `--oracle-test` (connexion, CRUD, clé étrangère, transactions) | **Exécuté : réussi** |
| Schéma Oracle complet (`oracle/01_schema.sql`) | **Exécuté sans erreur (14 tables)** |
| Les 5 modules sur Oracle (`--oracle`) : Employés, Production, Matières, Clients, Commandes | **Écrit, non compilé, non testé** (voir §7) — à valider par `--oracle-test-modules` |

Pourquoi « non compilé / non exécuté » : l'environnement où ce travail a été fait n'a ni Qt, ni compilateur Qt, ni Oracle,
ni accès réseau pour les installer. Rien n'a été validé par un build. **Le premier build et `--oracle-test` sur votre
machine sont les vrais tests** ; corrigez-moi les messages d'erreur éventuels et je les traiterai.

Pourquoi SQLite n'est pas remplacé : voir §3 — remplacer `QSQLITE` par `QODBC` ne suffit pas, la majorité du SQL
est spécifique à SQLite, et je ne peux pas le valider sans Oracle. Conformément à votre consigne de repli, Oracle est donc
ajouté **à côté** de SQLite, sans rien casser.

### Résultat réel de `--oracle-test` (Windows, Qt 6.7.3 MinGW 64-bit, Oracle 11g XE 11.2.0.2.0)

Connexion, SELECT/INSERT/UPDATE/DELETE avec valeurs liées, accents, DATE, TIMESTAMP, NULL, clés étrangères
(ORA-02291 / ORA-02292), COMMIT, ROLLBACK, fermeture et réouverture : **tout est OK**.
Une limite trouvée : **le pilote refuse les entiers 64 bits liés** (`qlonglong`/`qint64` → « QODBC: unknown error »).
Les dépôts du projet lient des `qint64` (ex. `EmployeeRepository::find(qint64)`) : lors de la migration, lier les
identifiants en `int` (ou en `double`/texte). Autre point confirmé : `lastInsertId()` est vide, utiliser `SEQUENCE.CURRVAL`.

## 2. Audit de l'architecture base de données

Trois connexions SQLite, créées dans `main.cpp` :

| Fichier | Connexion Qt | Ouverte par | Tables | Modules |
|---|---|---|---|---|
| `clients.sqlite` | **connexion par défaut** (sans nom) | `openClientDatabase()` | `clients`, `mesures`, `commandes`, `matieres`, `commandes_atelier`, `atelier_meta` | Clients (`ClientWindow`, `QSqlQuery` sans base passée), Commandes (`OrderWindow`), et `TaskStore::orders()` (Production) |
| `employes.sqlite` | nom = UUID (`Database`) | `Database::open()` | `employees`, `employee_attendance`, `employee_assignments`, `employee_metadata`, `tache`, `production_meta` | Employés (`EmployeeRepository`), Production (`TaskStore`, même connexion : `people()` lit `employees`) |
| `stock.sqlite` | nom = UUID (`InventoryRepository`) | `InventoryRepository::open()` | `materials`, `movements` | Matières |

Dans Oracle, un seul schéma (utilisateur `HSAN`) peut contenir les 14 tables : le script `oracle/01_schema.sql` les crée toutes.

### Dépendances SQLite à traiter (par fichier)

| Construction SQLite | Où | Équivalent Oracle / action |
|---|---|---|
| `QSQLITE`, `setDatabaseName(chemin)`, `PRAGMA foreign_keys / busy_timeout` | `database.cpp`, `inventoryrepository.cpp`, `clientdatabase.cpp` | `QODBC` + DSN ; clés étrangères toujours actives, pas de PRAGMA |
| `CREATE TABLE IF NOT EXISTS`, `CREATE INDEX IF NOT EXISTS` | `employeerepository.cpp:44-49`, `inventoryrepository.cpp:37-40`, `clientdatabase.cpp`, `orderwindow.cpp:595-606`, `productionstore.cpp:29-35` | Absent d'Oracle avant 23ai : schéma créé par script (`oracle/`), plus par l'application |
| `INTEGER PRIMARY KEY AUTOINCREMENT` + `lastInsertId()` | toutes les tables ; `employeerepository.cpp:92,212`, `inventoryrepository.cpp:106`, `productionstore.cpp:116` | Séquence + déclencheur (fait) ; **`lastInsertId()` est vide avec QODBC/Oracle** → lire `SEQ.CURRVAL` (ou `RETURNING id INTO`) |
| `DELETE FROM sqlite_sequence …` | `employeerepository.cpp:205` | `DROP/CREATE SEQUENCE` ou ne pas réinitialiser |
| `INSERT OR IGNORE` | `employeerepository.cpp:63`, `productionstore.cpp:170` | `MERGE` ou `INSERT … WHERE NOT EXISTS` |
| `INSERT … ON CONFLICT(employee_id,day) DO UPDATE` | `employeerepository.cpp` (`insertAttendance`) | `MERGE INTO … USING dual` |
| `COLLATE NOCASE` (colonnes uniques et `ORDER BY`) | `email`, `lot`, `ORDER BY name`, `nom` | Index unique sur `UPPER(col)` (fait) ; `ORDER BY NLSSORT(col,'NLS_SORT=BINARY_CI')` ou `UPPER(col)` ; recherche `findLot` : `UPPER(lot)=UPPER(?)` |
| `date('now','-20 day')`, `date('now','-1 year')`, `strftime('%m-%d', …)`, `'now','localtime'` | `clientdatabase.cpp`, `clientwindow.cpp:256-258,529,577` | `SYSDATE-20`, `ADD_MONTHS(SYSDATE,-12)`, `TO_CHAR(naissance,'MM-DD') = TO_CHAR(SYSDATE,'MM-DD')` |
| `SUM(categorie='VIP')` (booléen) | `clientwindow.cpp:256` | `SUM(CASE WHEN categorie='VIP' THEN 1 ELSE 0 END)` |
| `LIMIT 5` | `clientwindow.cpp:668` | `WHERE ROWNUM <= 5` (11g) ou `FETCH FIRST 5 ROWS ONLY` (12c+) |
| `CAST(id AS TEXT)` | `clientwindow.cpp:227` | `CAST(id AS VARCHAR2(20))` ou `TO_CHAR(id)` |
| `GROUP BY c.id` avec colonnes non agrégées | `clientwindow.cpp:586` | Oracle exige toutes les colonnes dans `GROUP BY` |
| Dates stockées en `TEXT` ISO (`QDate::toString(Qt::ISODate)` / `fromString`) | tous les modules | Colonnes `DATE`/`TIMESTAMP` : lier `QDate`/`QDateTime`, relire avec `.toDate()` / `.toDateTime()` |
| Booléens `0/1` | `available`, `completed` | `NUMBER(1)` + `CHECK` (fait) ; `.toBool()` reste valable |
| Message d'erreur `contains("UNIQUE")` | `employeerepository.cpp`, `inventoryrepository.cpp` | Oracle : `ORA-00001` |
| Mots réservés | colonnes `date` (mesures, commandes, movements) | Renommées `date_mesure`, `date_commande`, `mov_date` (fait dans le schéma) |
| **Chaîne vide = NULL** | colonnes `NOT NULL` fournies vides (`mail`, `tel`, raisons…) | Valider côté application avant l'`INSERT` ; sinon `ORA-01400` |
| Transactions `db.transaction()/commit()/rollback()` | repositories, `orderwindow.cpp:621-692`, `productionstore.cpp:151-168` | Compatibles ; en Oracle le DDL valide implicitement la transaction |
| `numRowsAffected()` utilisé pour détecter « n'existe plus » | `remove`, `completeAssignment`, … | Fonctionne avec QODBC (vérifié par `--oracle-test`) |
| `QSqlQuery` sans connexion (connexion par défaut) | `ClientWindow` (~40 requêtes), `productionstore.cpp:59-60` | À passer en connexion explicite lors de la migration |
| Requêtes du module Commandes qui supposent la base Clients (`FROM clients`) | `orderwindow.cpp:617,718` | Naturel en Oracle (même schéma) |

### Risques

1. Le SQL du module Clients (~40 requêtes sur la connexion par défaut, `strftime`, `LIMIT`, booléens) est le plus
   affecté ; les modules Employés et Matières sont davantage encapsulés dans un dépôt.
2. Les données existantes (fichiers `.sqlite`) ne sont **pas** migrées : conversion des dates ISO en `DATE` nécessaire.
3. Comportements qui changent avec Oracle : chaîne vide = NULL ; tri/recherche insensibles à la casse à réécrire ;
   `ON DELETE SET NULL` / `CASCADE` désormais appliqués par la base.

## 3. Stratégie retenue

1. **Maintenant** : Oracle coexiste avec SQLite. `OracleConnection` est disponible pour tout le code ; l'application
   continue d'utiliser SQLite ; `--oracle-test` valide votre environnement.
2. **Ensuite, module par module** (recommandé : Employés, puis Matières, Production, Commandes, Clients en dernier),
   en introduisant un dialecte (`Dialect::Sqlite/Oracle`) ou une fabrique de dépôts, et en testant chaque module sur Oracle
   avant de passer au suivant. Les tests existants (`tests/`) utilisent SQLite en mémoire et resteront valables.

## 4. Fichiers ajoutés / modifiés

| Fichier | Rôle |
|---|---|
| `common/oracleconnection.h/.cpp` (nouveau) | Singleton de Meyers, connexion `QODBC` au DSN, mot de passe lu dans l'environnement, messages d'erreur avec piste de résolution, aucune `QSqlDatabase` conservée, `shutdown()` explicite |
| `common/oraclecheck.h/.cpp` (nouveau) | Test de bout en bout sur des objets temporaires `AUREA_TST_*` |
| `main.cpp` | Option `--oracle-test` (n'ouvre aucune base SQLite) |
| `AureaAtelier.pro`, `CMakeLists.txt` | Ajout des deux fichiers (bibliothèque `oracle_core` en CMake). Aucun module Qt de plus : `sql` y était déjà |
| `oracle/01_schema.sql` | 14 tables, 11 séquences + déclencheurs, contraintes, index |
| `oracle/02_optional_constraints.sql` | 3 clés étrangères qui changent le comportement actuel — à décider |
| `oracle/99_drop_all.sql` | Suppression complète du schéma (manuel) |
| `.gitignore` | Exclut `*.pro.user` (contient les variables d'environnement), `*.sqlite`, `build/` |

Aucun fichier existant n'a été supprimé ; aucune base SQLite n'est touchée. L'archive d'origine reste votre sauvegarde.

## 5. Mise en route (Windows, Qt Creator)

1. **Qt 64 bits** : le kit doit être « MinGW 64-bit » ou « MSVC 64-bit » (pas 32 bits), comme le pilote ODBC et le DSN.
   Vérifiez que `…\Qt\<version>\<kit>\plugins\sqldrivers\qsqlodbc.dll` existe.
2. **Mot de passe** (jamais dans le code) : Qt Creator → *Projets* → *Exécuter* → *Environnement d'exécution* → *Ajouter* :
   `AUREA_ORACLE_PASSWORD = <votre mot de passe>`. Pour les autres valeurs, les défauts conviennent :
   `AUREA_ORACLE_DSN=Source_Projet2A`, `AUREA_ORACLE_USER=hsan`.
   Le mot de passe est enregistré dans le fichier local `*.pro.user` : ne le poussez pas sur GitHub (`.gitignore` fourni).
3. **Schéma** : dans SQL Developer ou SQL*Plus, connecté en `HSAN` : `@oracle/01_schema.sql`. Vérifiez qu'il n'y a pas d'erreur
   (`SELECT table_name FROM user_tables;` doit lister 14 tables). Votre pilote « Oracle in XE » suggère Oracle XE 11g : le
   script est écrit pour 11g et plus.
4. **Reconstruire** : *Compiler → Nettoyer tout*, puis *Exécuter qmake* (ou *Reconfigurer CMake*), puis *Compiler*.
5. **Tester** : *Projets* → *Exécuter* → *Arguments de ligne de commande* : `--oracle-test`, puis lancez. Une fenêtre
   affiche le rapport (aussi sur la sortie standard). Il doit se terminer par « tous les contrôles sont passés ».
   Le test nécessite les droits `CREATE TABLE` et `CREATE SEQUENCE` ; il crée puis supprime `AUREA_TST_PARENT`,
   `AUREA_TST_CHILD`, `AUREA_TST_SEQ`.

### Dépannage rapide

| Message | Cause probable |
|---|---|
| « pilote QODBC introuvable » | `qsqlodbc.dll` absent du kit Qt, ou kit incompatible |
| `IM014` / « architecture mismatch » | application 32 bits face à un DSN 64 bits (ou l'inverse) |
| `IM002` | nom de DSN incorrect, ou DSN créé dans l'outil ODBC 32 bits |
| `ORA-01017` | utilisateur / mot de passe incorrects |
| `ORA-12541`, `ORA-12514` | listener ou base XE arrêtés |
| `ORA-01031` / `ORA-01950` pendant le test | droits ou quota insuffisants pour `HSAN` |

## 6. Vérifications : exécutées / à faire chez vous

| Vérification | Statut |
|---|---|
| Lecture complète du code et audit SQL | Faite |
| Génération du schéma ; comptage des tables, séquences, déclencheurs ; noms ≤ 30 caractères | Faite (scripts) |
| Compilation du projet (qmake / CMake) | **À faire chez vous** — impossible ici |
| `--oracle-test` (connexion, CRUD, FK, transactions, fermeture/réouverture) | **À faire chez vous** |
| Exécution de `01_schema.sql` | **À faire chez vous** |
| Tests CRUD par module sur Oracle | **Sans objet tant que les modules ne sont pas migrés** |
| Fonctionnement de l'application existante (SQLite) | Non modifié par conception ; à confirmer au premier lancement |

## 7. Les trois modules sur Oracle (option `--oracle`)

**SQLite reste le mode par défaut** : sans option, l'application fonctionne exactement comme avant (deux fichiers `.sqlite`).
Avec `--oracle`, tous les modules utilisent la base Oracle (schéma `HSAN`, 8 tables) à travers **une seule connexion**, celle du singleton
`OracleConnection`. 
| Fichier | Changement |
|---|---|
| `common/sqldialect.h` | Dialecte déduit du pilote (`QODBC` = Oracle) : identifiants liés en `int`, dates en `QDate`/`QDateTime`, `isoDate()`, `SEQUENCE.CURRVAL` à la place de `lastInsertId()` |
| `common/oracleconnection.*` | Mode connexion par défaut ; `connectionName()` |
| `employees/employeerepository.*`, `employees/database.*` | Employés : double dialecte (voir ci-dessous), `Database::openOracle()` |
| `production/productionstore.cpp` | Production : même démarche pour `tache` et `production_meta` |
| `materials/inventoryrepository.*` | Matières : `openOracle()`, colonne `mov_date`, `TIMESTAMP` relu en UTC au même format qu'avec SQLite, `UPPER()` pour les tris et la recherche de lot, `ORA-00001` |
| `main.cpp` | Options `--oracle` et `--oracle-test-modules` ; `OracleConnection::shutdown()` à la sortie |
| `common/oraclemodulecheck.*` | Test CRUD des trois modules sur les vraies tables, avec nettoyage |

Correspondances appliquées : `INSERT OR IGNORE` → `INSERT … WHERE NOT EXISTS` ; `ON CONFLICT … DO UPDATE` → `UPDATE` puis `INSERT` ;
`COLLATE NOCASE` → `UPPER()` ; `strftime('%m-%d', …)` → `TO_CHAR(…, 'MM-DD')` / `EXTRACT(MONTH …)` ; `LIMIT 5` → `ROWNUM <= 5` ;
`date('now','-1 year')` → date limite liée en paramètre ; `lastInsertId()` → `SEQUENCE.CURRVAL`.

### Tester (dans l'ordre)
1. `oracle/01_schema.sql` déjà exécuté (8 tables).
2. Argument `--oracle-test-modules` : écrit dans les vraies tables des cinq modules (employé, tâche, pointage, matière + mouvements, client,
   mesures, commandes d'achat, commande d'atelier ; tous reconnaissables : adresses `…@aurea.invalid`, lots `ORA-TST-…`), vérifie chaque relecture,
   la recherche, le tableau de bord, le classement, les clés étrangères et les transactions, puis supprime tout. Succès : « les trois modules fonctionnent sur Oracle ».
3. Argument `--oracle` : lance l'application complète sur Oracle. Au premier lancement, les données d'exemple de chaque module sont insérées
   dans vos tables Oracle (comme avec SQLite).
4. Sans argument : retour à SQLite.

### Limites connues
- Les données SQLite existantes ne sont pas copiées vers Oracle (le premier lancement charge les données d'exemple).
- La clé étrangère de `oracle/02_optional_constraints.sql` n'est pas activée : elles changeraient le comportement (suppression d'un client ayant des commandes d'atelier).
- Les tests automatiques de `tests/` restent sur SQLite en mémoire.
