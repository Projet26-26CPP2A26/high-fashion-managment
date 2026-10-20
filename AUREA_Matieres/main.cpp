#include "inventoryrepository.h"
#include "materialwindow.h"
#include "oraclecheck.h"
#include "oracleconnection.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QLocale>
#include <QMessageBox>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>
#include <cstdio>

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    if (qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR"))
        qputenv("QT_QPA_FONTDIR", QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts").toUtf8());
#endif
    QApplication app(argc, argv);
    app.setApplicationName("AUREA");
    app.setOrganizationName("Velora");
    app.setApplicationDisplayName("AUREA — Gestion des matières");
    app.setWindowIcon(QIcon(":/assets/logo_aurea.png"));
    app.setStyle("Fusion");
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));

    for (const QString &file : QStringList{"segoeui.ttf", "segoeuib.ttf", "georgia.ttf"})
        QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts/" + file));
    for (const QString &file : QStringList{"Inter.ttf", "CormorantGaramond.ttf", "CormorantGaramond-Italic.ttf"})
        QFontDatabase::addApplicationFont(":/assets/fonts/" + file);
    app.setFont(QFont("Inter", 10));
    app.setStyleSheet("QToolTip { background: #eee2bf; color: #23292b; border: none; padding: 7px; }");

    QCommandLineParser parser;
    parser.setApplicationDescription("AUREA — Gestion des matières : stock, lots & QR, mouvements (C++ / Qt Widgets)");
    parser.addHelpOption();
    parser.addOption({"data-dir", "Dossier de la base SQLite du stock (stock.sqlite).", "path"});
    parser.addOption({"demo", "Utiliser une base de démonstration distincte."});
    parser.addOption({"render", "Capturer les écrans puis quitter.", "directory"});
    parser.addOption({"oracle-test", "Tester la connexion Oracle (ODBC, DSN Source_Projet2A) puis quitter."});
    parser.addOption({"oracle", "Utiliser Oracle (tables materials et movements) au lieu de SQLite."});
    parser.process(app);

    if (parser.isSet("oracle-test")) {   // aucune base SQLite n'est ouverte : test isolé
        const OracleCheck::Report report = OracleCheck::run();
        std::fprintf(stdout, "%s\n", qPrintable(report.text));
        std::fflush(stdout);
        QMessageBox box(report.ok ? QMessageBox::Information : QMessageBox::Warning, "AUREA — test Oracle",
                        report.text, QMessageBox::Ok);
        box.setTextFormat(Qt::PlainText);
        box.exec();
        return report.ok ? 0 : 2;
    }

    QString dataDir = parser.value("data-dir");
    if (dataDir.isEmpty())
        dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (parser.isSet("demo"))
        dataDir += "/demo";
    QDir().mkpath(dataDir);

    const bool useOracle = parser.isSet("oracle");
    struct OracleShutdown { ~OracleShutdown() { OracleConnection::instance().shutdown(); } } oracleShutdown;   // détruit APRÈS le dépôt et la fenêtre (ordre inverse de déclaration)

    InventoryRepository stockRepository;   // exemples chargés une seule fois si la base est vide
    if (!(useOracle ? stockRepository.openOracle(true) : stockRepository.open(QDir(dataDir).filePath("stock.sqlite"), true))) {
        if (parser.isSet("render")) std::fprintf(stderr, "Stock database error: %s\n", qPrintable(stockRepository.error()));
        else QMessageBox::critical(nullptr, "AUREA", stockRepository.error());
        return 1;
    }

    MaterialWindow window(stockRepository);
    window.setWindowTitle("AUREA — Gestion des matières");
    if (QScreen *screen = app.primaryScreen()) {
        const QSize available = screen->availableGeometry().size() - QSize(40, 70);
        window.resize(std::min(1480, available.width()), std::min(940, available.height()));
    }
    window.show();
    if (parser.isSet("render"))
        QTimer::singleShot(300, &app, [&] { window.renderPreviews(parser.value("render")); app.quit(); });
    return app.exec();
}
