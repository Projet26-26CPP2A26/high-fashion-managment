#include "appsetup.h"
#include "clientdatabase.h"
#include "orderwindow.h"
#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QScreen>
#include <algorithm>
#include <QSqlDatabase>
#include <QSqlError>
#include <QDebug>
#include <QSqlQuery>

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    if (qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR"))
        qputenv("QT_QPA_FONTDIR", QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts").toUtf8());
#endif
    QApplication app(argc, argv);
    qDebug() << "Drivers disponibles :" << QSqlDatabase::drivers();
    const QString dataDir = setupAurea(app, QStringLiteral("AUREA — Gestion des commandes"));

    // Les clients (et leur statut VIP, qui déclenche la remise du devis) viennent de la base clients.sqlite.
    if (!openClientDatabase(QDir(dataDir).filePath("clients.sqlite"))) {
        QMessageBox::critical(nullptr, "AUREA", "Impossible d'ouvrir la base de données des clients.");
        return 1;
    }

    // Connexion Oracle (nommée, pour ne pas écraser la connexion SQLite des clients)
    QSqlDatabase oracle = QSqlDatabase::addDatabase("QODBC", "oracle");
    oracle.setDatabaseName(
        "DRIVER={Oracle in XE};"
        "DBQ=localhost:1521/XE;"
        "UID=yasmine;"
        "PWD=TON PASSWORD;"
        );

    if (oracle.open())
        qDebug() << "Connexion Oracle OK";
    else
        qDebug() << "Erreur connexion Oracle :" << oracle.lastError().text();


    auto *window = new OrderWindow;
    window->setWindowTitle(QStringLiteral("AUREA — Gestion des commandes"));

    // Disposition d'origine : colonne de boutons verticale à gauche, barre du haut, page.
    if (QScreen *screen = app.primaryScreen()) {
        const QSize available = screen->availableGeometry().size() - QSize(40, 70);
        window->resize(std::min(1480, available.width()), std::min(940, available.height()));
    }
    window->show();
    return app.exec();
}