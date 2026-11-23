DECLARE
  PROCEDURE add_emp(p_name VARCHAR2, p_email VARCHAR2, p_position VARCHAR2, p_specialty VARCHAR2,
                    p_hired DATE, p_available NUMBER, p_load NUMBER) IS
  BEGIN
    INSERT INTO employees(id, name, email, position, specialty, hired, available, base_load)
    SELECT (SELECT COALESCE(MAX(id), 0) + 1 FROM employees),
           p_name, p_email, p_position, p_specialty, p_hired, p_available, p_load
    FROM dual
    WHERE NOT EXISTS (SELECT 1 FROM employees WHERE UPPER(email) = UPPER(p_email));
  END;
BEGIN
  add_emp('Sarah Ben Ali',    'sarah.benali@demo.invalid',    'Couturier',           'Couture haute précision', DATE '2019-03-11', 1, 35);
  add_emp('Nour Hammami',     'nour.hammami@demo.invalid',    'Brodeur',             'Broderie main',           DATE '2020-09-01', 1, 20);
  add_emp('Youssef Trabelsi', 'youssef.trabelsi@demo.invalid','Chef d''atelier',     'Contrôle qualité',        DATE '2015-01-19', 1, 60);
  add_emp('Amira Gharbi',     'amira.gharbi@demo.invalid',    'Modéliste',           'Patronage',               DATE '2018-06-04', 1, 45);
  add_emp('Karim Jaziri',     'karim.jaziri@demo.invalid',    'Coupeur',             'Patronage',               DATE '2021-02-15', 1, 10);
  add_emp('Leila Mansour',    'leila.mansour@demo.invalid',   'Finisseur',           'Finitions',               DATE '2022-10-03', 1, 0);
  add_emp('Mehdi Bouazizi',   'mehdi.bouazizi@demo.invalid',  'Couturier',           'Drapé et moulage',        DATE '2017-04-24', 1, 95);
  add_emp('Ines Chaabane',    'ines.chaabane@demo.invalid',   'Responsable qualité', 'Contrôle qualité',        DATE '2016-11-07', 1, 50);
  add_emp('Rania Sassi',      'rania.sassi@demo.invalid',     'Brodeur',             'Broderie main',           DATE '2023-05-22', 0, 30);
  add_emp('Walid Khelifi',    'walid.khelifi@demo.invalid',   'Coupeur',             'Couture haute précision', DATE '2020-01-13', 1, 70);
  add_emp('Salma Dridi',      'salma.dridi@demo.invalid',     'Modéliste',           'Drapé et moulage',        DATE '2024-02-05', 1, 15);
  add_emp('Hichem Ayari',     'hichem.ayari@demo.invalid',    'Finisseur',           'Finitions',               DATE '2021-08-30', 0, 90);
  add_emp('Olfa Mejri',       'olfa.mejri@demo.invalid',      'Couturier',           'Couture haute précision', DATE '2025-03-17', 1, 25);
  add_emp('Tarek Zouari',     'tarek.zouari@demo.invalid',    'Couturier',           'Finitions',               DATE '2014-09-15', 1, 100);
  add_emp('Dorra Belhadj',    'dorra.belhadj@demo.invalid',   'Modéliste',           'Patronage',               DATE '2022-01-10', 1, 40);
END;
/
COMMIT;
