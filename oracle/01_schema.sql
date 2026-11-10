-- ============================================================================
-- AUREA - Gestion des matieres : schema Oracle (Oracle 11g XE et superieur)
-- Executer connecte en tant que IYED (base enfichable XEPDB1, Oracle XE 21c) :
--   SQL*Plus      : sqlplus Iyed/Esprit18@XEPDB1  puis  @01_schema.sql
--   SQL Developer : ouvrir le fichier puis " Executer le script " (F5)
-- Pas de colonne IDENTITY (absente avant 12c) : sequence + declencheur BEFORE INSERT par table.
-- Table : materials (stock).
-- ============================================================================

-- ---------- Module Matieres (ex stock.sqlite) ----------
CREATE TABLE materials (
  id       NUMBER(10)          NOT NULL,
  name     VARCHAR2(120 CHAR)  NOT NULL,
  type     VARCHAR2(100 CHAR)  NOT NULL,
  color    VARCHAR2(60 CHAR)   NOT NULL,
  quantity NUMBER(12,2)        NOT NULL,
  minimum  NUMBER(12,2)        NOT NULL,
  unit     VARCHAR2(20 CHAR)   NOT NULL,
  price    NUMBER(12,2)        NOT NULL,
  supplier VARCHAR2(120 CHAR)  NOT NULL,
  lot      VARCHAR2(80 CHAR)   NOT NULL,
  CONSTRAINT pk_materials PRIMARY KEY (id),
  CONSTRAINT ck_materials_quantity CHECK (quantity >= 0),
  CONSTRAINT ck_materials_minimum CHECK (minimum >= 0),
  CONSTRAINT ck_materials_price CHECK (price >= 0)
);
-- SQLite : lot UNIQUE COLLATE NOCASE
CREATE UNIQUE INDEX uq_materials_lot ON materials(UPPER(lot));
CREATE INDEX idx_materials_type ON materials(type);
CREATE SEQUENCE seq_materials START WITH 1 INCREMENT BY 1 NOCACHE;
CREATE OR REPLACE TRIGGER trg_materials_bi
BEFORE INSERT ON materials FOR EACH ROW
WHEN (NEW.id IS NULL)
BEGIN
  SELECT seq_materials.NEXTVAL INTO :NEW.id FROM dual;
END;
/
COMMIT;
