#pragma once
#include <QString>

// Ouvre (et crée au besoin) la base SQLite du module Clients.
// La connexion est la connexion Qt « par défaut » : le module Clients l'utilise
// directement avec QSqlQuery, sans passer de QSqlDatabase en paramètre.
// Au premier lancement, des clients d'exemple sont insérés.
bool openClientDatabase(const QString &path);
