CREATE TABLE employees (
  id        NUMBER(10)          NOT NULL,
  name      VARCHAR2(120 CHAR)  NOT NULL,
  email     VARCHAR2(180 CHAR)  NOT NULL,
  position  VARCHAR2(100 CHAR)  NOT NULL,
  specialty VARCHAR2(100 CHAR)  NOT NULL,
  hired     DATE                NOT NULL,
  available NUMBER(1)           NOT NULL,
  base_load NUMBER(3)           NOT NULL,
  CONSTRAINT pk_employees PRIMARY KEY (id) USING INDEX TABLESPACE users,
  CONSTRAINT ck_employees_available CHECK (available IN (0,1)),
  CONSTRAINT ck_employees_base_load CHECK (base_load BETWEEN 0 AND 100)
) TABLESPACE users;

CREATE SEQUENCE seq_employees START WITH 1 INCREMENT BY 1 NOCACHE;

COMMIT;
