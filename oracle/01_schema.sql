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

COMMIT;
