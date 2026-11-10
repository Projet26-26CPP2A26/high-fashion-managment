#include "inventoryrepository.h"
#include "materialwindow.h"
#include "oraclecheck.h"
#include "oracleconnection.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFontDatabase>
#include <QIcon>
#include <QLocale>
#include <QMessageBox>
#include <QScreen>
#include <QStandardPaths>
#include <cstdio>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("AUREA");
    app.setOrganizationName("Velora");   // fixe le dossier de la base SQLite (AppData\Local\Velora\AUREA)
    app.setApplicationDisplayName("AUREA — Gestion des matières");
    app.setWindowIcon(QIcon(":/assets/logo_aurea.png"));
    app.setStyle("Fusion");
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));

    // Polices embarquées dans les ressources (materials.qrc)
    for (const QString &file : QStringList{"Inter.ttf", "CormorantGaramond.ttf", "CormorantGaramond-Italic.ttf"})
        QFontDatabase::addApplicationFont(":/assets/fonts/" + file);
    app.setFont(QFont("Inter", 10));
    app.setStyleSheet("QToolTip { background: #eee2bf; color: #23292b; border: none; padding: 7px; }");

    // Options de la ligne de commande
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"sqlite", "Utiliser la base locale SQLite (stock.sqlite) au lieu d'Oracle."});
    parser.addOption({"data-dir", "Dossier de la base SQLite.", "path"});
    parser.addOption({"demo", "Utiliser une base SQLite de démonstration distincte."});
    parser.addOption({"oracle-test", "Tester la connexion Oracle puis quitter."});
    parser.process(app);

    // --oracle-test : affiche le rapport du test puis quitte.
    if (parser.isSet("oracle-test")) {
        const OracleCheck::Report report = OracleCheck::run();
        std::printf("%s\n", qPrintable(report.text));
        std::fflush(stdout);
        QMessageBox::information(nullptr, "AUREA — test Oracle", report.text);
        return report.ok ? 0 : 2;
    }

    // Base de données : Oracle par défaut, SQLite avec --sqlite.
    const bool useOracle = !parser.isSet("sqlite");
    if (useOracle) {
        OracleConnection &c = OracleConnection::createInstance();   // le singleton
        if (!c.createConnection()) {
            QMessageBox::critical(nullptr, "Base de données", "Échec de la connexion.\n\n" + c.errorMessage());
            return 1;
        }
    }

    int result = 1;
    {   // bloc : le dépôt et la fenêtre sont détruits AVANT la fermeture d'Oracle
        InventoryRepository repository;
        bool opened = false;
        if (useOracle) {
            opened = repository.openOracle(true);
        } else {
            QString folder = parser.value("data-dir");
            if (folder.isEmpty()) folder = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
            if (parser.isSet("demo")) folder += "/demo";
            QDir().mkpath(folder);
            opened = repository.open(QDir(folder).filePath("stock.sqlite"), true);
        }
        if (!opened) {
            QMessageBox::critical(nullptr, "AUREA", repository.error());
            return 1;
        }

        MaterialWindow window(repository);
        window.setWindowTitle("AUREA — Gestion des matières");
        const QSize screen = app.primaryScreen()->availableGeometry().size();   // sans dépasser de l'écran
        window.resize(qMin(1480, screen.width() - 40), qMin(940, screen.height() - 70));
        window.show();
        if (useOracle)
            QMessageBox::information(&window, "Base de données", "Connexion réussie.\nOracle — source " + OracleConnection::instance().dsn() + ".");
        result = app.exec();
    }
    if (useOracle) OracleConnection::instance().shutdown();
    return result;
}
