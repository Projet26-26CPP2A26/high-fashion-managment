-- ============================================================================
-- AUREA - Gestion des matieres : VUES Oracle (une par page de l'application)
-- A executer apres 01_schema.sql, connecte en tant que IYED :
--   SQL Developer : ouvrir le fichier puis " Executer le script " (F5)
-- Une vue = une requete enregistree. Elle apparait dans SQL Developer sous " Vues "
-- et s'ouvre comme une table (onglet " Donnees "), mais ne stocke aucune donnee :
-- elle est recalculee a partir de MATERIALS a chaque ouverture.
-- ============================================================================

-- Page " Matieres " : toutes les matieres, avec leur etat et la valeur du stock
CREATE OR REPLACE VIEW v_matieres AS
SELECT id, name AS matiere, type, color AS couleur, lot, supplier AS fournisseur,
       quantity AS stock, minimum AS seuil, unit AS unite, price AS prix,
       quantity * price AS valeur,
       CASE WHEN quantity = 0 THEN 'Rupture'
            WHEN quantity < minimum THEN 'A commander'
            ELSE 'Disponible' END AS etat
FROM materials
ORDER BY name;

-- Page " A commander " : matieres sous le seuil, et quantite a commander
CREATE OR REPLACE VIEW v_alertes AS
SELECT id, name AS matiere, supplier AS fournisseur, lot,
       quantity AS stock, minimum AS seuil, unit AS unite,
       minimum - quantity AS a_commander
FROM materials
WHERE quantity < minimum
ORDER BY name;

-- Page " Statistiques " : par type, nombre de matieres, nombre sous le seuil, valeur du stock
CREATE OR REPLACE VIEW v_statistiques AS
SELECT type,
       COUNT(*) AS nb_matieres,
       SUM(CASE WHEN quantity < minimum THEN 1 ELSE 0 END) AS sous_le_seuil,
       SUM(quantity * price) AS valeur
FROM materials
GROUP BY type
ORDER BY type;
