#pragma once
#include <QString>

// Test de bout en bout de la connexion Oracle (lancé par « AureaAtelier --oracle-test »).
// Il utilise trois objets temporaires préfixés AUREA_TST_ (créés puis supprimés) : il ne touche
// à aucune table de l'application.
namespace OracleCheck {
struct Report {
    bool ok = false;
    QString text;   // rapport lisible, une ligne par vérification
};
Report run();
}
