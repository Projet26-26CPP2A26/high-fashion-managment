-- ============================================================================
-- AUREA - Gestion des matieres : SUPPRESSION des tables (donnees comprises). IRREVERSIBLE.
-- A lancer a la main, uniquement pour repartir de zero. Les erreurs ORA-00942 / ORA-02289 sont sans gravite.
-- ============================================================================
DROP VIEW v_statistiques;
DROP VIEW v_alertes;
DROP VIEW v_matieres;
DROP TABLE materials CASCADE CONSTRAINTS PURGE;
DROP SEQUENCE seq_materials;
