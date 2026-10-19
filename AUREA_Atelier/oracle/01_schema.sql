-- ============================================================================
-- AUREA - schema Oracle (compatible Oracle 11g XE et versions superieures)
-- Executer connecte en tant que HSAN, sur une base vierge :
--   SQL*Plus      : @01_schema.sql
--   SQL Developer : ouvrir le fichier puis " Executer le script " (F5)
-- Pas de colonne IDENTITY (absente avant 12c) : sequence + declencheur BEFORE INSERT par table.
-- Les identifiants Oracle sont limites a 30 caracteres (11g).
-- Correspondance SQLite -> Oracle : TEXT->VARCHAR2(n CHAR), INTEGER->NUMBER(n), REAL->NUMBER(p,2),
--   dates ISO yyyy-MM-dd->DATE, horodatages ISO->TIMESTAMP, booleens 0/1->NUMBER(1) + CHECK.
-- Colonnes renommees (DATE est un mot reserve Oracle) :
--   movements.date->mov_date
-- ============================================================================

-- ---------- Module Employes (ex employes.sqlite) ----------
CREATE TABLE employees (
  id        NUMBER(10)          NOT NULL,
  name      VARCHAR2(120 CHAR)  NOT NULL,
  email     VARCHAR2(180 CHAR)  NOT NULL,
  position  VARCHAR2(100 CHAR)  NOT NULL,
  specialty VARCHAR2(100 CHAR)  NOT NULL,
  hired     DATE                NOT NULL,
  available NUMBER(1)           NOT NULL,
  base_load NUMBER(3)           NOT NULL,
  CONSTRAINT pk_employees PRIMARY KEY (id),
  CONSTRAINT ck_employees_available CHECK (available IN (0,1)),
  CONSTRAINT ck_employees_base_load CHECK (base_load BETWEEN 0 AND 100)
);
-- SQLite : email UNIQUE COLLATE NOCASE -> index unique sur la valeur en majuscules
CREATE UNIQUE INDEX uq_employees_email ON employees(UPPER(email));
CREATE SEQUENCE seq_employees START WITH 1 INCREMENT BY 1 NOCACHE;
CREATE OR REPLACE TRIGGER trg_employees_bi
BEFORE INSERT ON employees FOR EACH ROW
WHEN (NEW.id IS NULL)
BEGIN
  SELECT seq_employees.NEXTVAL INTO :NEW.id FROM dual;
END;
/
CREATE TABLE employee_attendance (
  id          NUMBER(10)          NOT NULL,
  employee_id NUMBER(10),
  name        VARCHAR2(120 CHAR)  NOT NULL,
  day         DATE                NOT NULL,
  arrival     VARCHAR2(5)         NOT NULL,   -- 'HH:mm', comme dans l'application
  departure   VARCHAR2(5),
  pause       NUMBER(3)           NOT NULL,
  CONSTRAINT pk_employee_attendance PRIMARY KEY (id),
  CONSTRAINT fk_attendance_employee FOREIGN KEY (employee_id) REFERENCES employees(id) ON DELETE SET NULL,
  CONSTRAINT ck_attendance_pause CHECK (pause BETWEEN 0 AND 240),
  CONSTRAINT uq_attendance_emp_day UNIQUE (employee_id, day)
);
CREATE INDEX idx_attendance_day ON employee_attendance(day);
CREATE SEQUENCE seq_attendance START WITH 1 INCREMENT BY 1 NOCACHE;
CREATE OR REPLACE TRIGGER trg_attendance_bi
BEFORE INSERT ON employee_attendance FOR EACH ROW
WHEN (NEW.id IS NULL)
BEGIN
  SELECT seq_attendance.NEXTVAL INTO :NEW.id FROM dual;
END;
/
CREATE TABLE employee_assignments (
  id          NUMBER(10)          NOT NULL,
  employee_id NUMBER(10),
  name        VARCHAR2(120 CHAR)  NOT NULL,
  title       VARCHAR2(200 CHAR)  NOT NULL,
  specialty   VARCHAR2(100 CHAR)  NOT NULL,
  hours       NUMBER(3)           NOT NULL,
  load        NUMBER(4)           NOT NULL,
  created     TIMESTAMP(0)        NOT NULL,
  completed   NUMBER(1)           DEFAULT 0 NOT NULL,
  CONSTRAINT pk_employee_assignments PRIMARY KEY (id),
  CONSTRAINT fk_assignment_employee FOREIGN KEY (employee_id) REFERENCES employees(id) ON DELETE SET NULL,
  CONSTRAINT ck_assignment_hours CHECK (hours BETWEEN 1 AND 80),
  CONSTRAINT ck_assignment_completed CHECK (completed IN (0,1))
);
CREATE INDEX idx_assignment_employee ON employee_assignments(employee_id);
CREATE SEQUENCE seq_assignments START WITH 1 INCREMENT BY 1 NOCACHE;
CREATE OR REPLACE TRIGGER trg_assignments_bi
BEFORE INSERT ON employee_assignments FOR EACH ROW
WHEN (NEW.id IS NULL)
BEGIN
  SELECT seq_assignments.NEXTVAL INTO :NEW.id FROM dual;
END;
/
CREATE TABLE employee_metadata (
  key   VARCHAR2(100 CHAR)   NOT NULL,
  value VARCHAR2(4000 CHAR)  NOT NULL,
  CONSTRAINT pk_employee_metadata PRIMARY KEY (key)
);

-- ---------- Module Production (tables rangees dans employes.sqlite) ----------
CREATE TABLE tache (
  id              NUMBER(10)         NOT NULL,
  id_commande     NUMBER(10)         NOT NULL,
  etape           VARCHAR2(100 CHAR) NOT NULL,
  id_employe      NUMBER(10),
  date_debut      DATE               NOT NULL,
  date_fin_prevue DATE               NOT NULL,
  priorite        VARCHAR2(10 CHAR)  NOT NULL,
  avancement      NUMBER(3)          NOT NULL,
  CONSTRAINT pk_tache PRIMARY KEY (id),
  CONSTRAINT ck_tache_priorite CHECK (priorite IN ('Haute','Moyenne','Basse')),
  CONSTRAINT ck_tache_avancement CHECK (avancement BETWEEN 0 AND 100)
);
CREATE INDEX idx_tache_commande ON tache(id_commande);
CREATE INDEX idx_tache_employe ON tache(id_employe);
CREATE SEQUENCE seq_tache START WITH 1 INCREMENT BY 1 NOCACHE;
CREATE OR REPLACE TRIGGER trg_tache_bi
BEFORE INSERT ON tache FOR EACH ROW
WHEN (NEW.id IS NULL)
BEGIN
  SELECT seq_tache.NEXTVAL INTO :NEW.id FROM dual;
END;
/
CREATE TABLE production_meta (
  key   VARCHAR2(100 CHAR)   NOT NULL,
  value VARCHAR2(4000 CHAR)  NOT NULL,
  CONSTRAINT pk_production_meta PRIMARY KEY (key)
);

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
CREATE TABLE movements (
  id          NUMBER(10)          NOT NULL,
  material_id NUMBER(10),
  mov_date    TIMESTAMP(3)        NOT NULL,   -- UTC, comme l'application (QDateTime::currentDateTimeUtc)
  name        VARCHAR2(120 CHAR)  NOT NULL,
  lot         VARCHAR2(80 CHAR)   NOT NULL,
  kind        VARCHAR2(30 CHAR)   NOT NULL,
  delta       NUMBER(12,2)        NOT NULL,
  unit        VARCHAR2(20 CHAR)   NOT NULL,
  reason      VARCHAR2(500 CHAR)  NOT NULL,
  CONSTRAINT pk_movements PRIMARY KEY (id),
  CONSTRAINT fk_movements_material FOREIGN KEY (material_id) REFERENCES materials(id) ON DELETE SET NULL
);
CREATE INDEX idx_movements_material ON movements(material_id);
CREATE SEQUENCE seq_movements START WITH 1 INCREMENT BY 1 NOCACHE;
CREATE OR REPLACE TRIGGER trg_movements_bi
BEFORE INSERT ON movements FOR EACH ROW
WHEN (NEW.id IS NULL)
BEGIN
  SELECT seq_movements.NEXTVAL INTO :NEW.id FROM dual;
END;
/

COMMIT;
