-- ============================================================================
-- AUREA - SUPPRESSION de tout le schema Oracle (donnees comprises). IRREVERSIBLE.
-- A lancer a la main, uniquement pour repartir de zero. Les erreurs ORA-00942 / ORA-02289 sont sans gravite.
-- Les bases SQLite de l'application ne sont pas concernees.
-- ============================================================================
DROP TABLE movements CASCADE CONSTRAINTS PURGE;
DROP TABLE materials CASCADE CONSTRAINTS PURGE;
DROP TABLE production_meta CASCADE CONSTRAINTS PURGE;
DROP TABLE tache CASCADE CONSTRAINTS PURGE;
DROP TABLE employee_metadata CASCADE CONSTRAINTS PURGE;
DROP TABLE employee_assignments CASCADE CONSTRAINTS PURGE;
DROP TABLE employee_attendance CASCADE CONSTRAINTS PURGE;
DROP TABLE employees CASCADE CONSTRAINTS PURGE;
DROP SEQUENCE seq_movements;
DROP SEQUENCE seq_materials;
DROP SEQUENCE seq_tache;
DROP SEQUENCE seq_assignments;
DROP SEQUENCE seq_attendance;
DROP SEQUENCE seq_employees;
