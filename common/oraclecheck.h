#pragma once
#include <QString>

// Test de la connexion Oracle (lancé par « AureaMatieres --oracle-test »).
// Il ne fait que lire : aucune donnée n'est modifiée.
namespace OracleCheck {
struct Report {
    bool ok = false;
    QString text;   // rapport lisible, une ligne par vérification
};
Report run();
}
