CREATE TABLE employee_badges (
  uid_badge   VARCHAR2(20 CHAR) NOT NULL,
  employee_id NUMBER(10)        NOT NULL,
  CONSTRAINT pk_employee_badges PRIMARY KEY (uid_badge) USING INDEX TABLESPACE users,
  CONSTRAINT uq_employee_badges_emp UNIQUE (employee_id) USING INDEX TABLESPACE users,
  CONSTRAINT fk_employee_badges_emp FOREIGN KEY (employee_id) REFERENCES employees(id) ON DELETE CASCADE
) TABLESPACE users;

CREATE TABLE badge_scans (
  id          NUMBER(10)        NOT NULL,
  uid_badge   VARCHAR2(20 CHAR) NOT NULL,
  employee_id NUMBER(10),
  sens        VARCHAR2(10 CHAR) NOT NULL,
  date_scan   DATE              DEFAULT SYSDATE NOT NULL,
  CONSTRAINT pk_badge_scans PRIMARY KEY (id) USING INDEX TABLESPACE users,
  CONSTRAINT ck_badge_scans_sens CHECK (sens IN ('ARRIVEE','DEPART','REFUSE')),
  CONSTRAINT fk_badge_scans_emp FOREIGN KEY (employee_id) REFERENCES employees(id) ON DELETE SET NULL
) TABLESPACE users;

CREATE INDEX idx_badge_scans_emp ON badge_scans(employee_id, date_scan) TABLESPACE users;

CREATE SEQUENCE seq_badge_scans START WITH 1 INCREMENT BY 1 NOCACHE;

COMMIT;
