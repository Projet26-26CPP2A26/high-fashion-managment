#pragma once
#include "oraclecheck.h"

namespace OracleCheck {
// Test CRUD des trois modules (Employés, Production, Matières) sur Oracle (« AureaAtelier --oracle-test-modules »).
// Il écrit dans les VRAIES tables (employees, employee_attendance, employee_assignments, tache, materials, movements) avec des
// données reconnaissables (adresse « .invalid »), puis supprime tout ce qu'il a créé. À lancer après
// oracle/01_schema.sql. Aucune donnée existante n'est modifiée.
Report runModules();
}
