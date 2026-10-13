#include "database.h"
#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>
#include <QLocale>
#include <QMessageBox>
#include <QPalette>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("AureaProduction");
    app.setOrganizationName("Velora");
    app.setApplicationDisplayName("AUREA — Gestion de la production");
    app.setWindowIcon(QIcon(":/assets/logo_aurea.png"));
    app.setStyle("Fusion");
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));

    for (const QString &f : {"Inter.ttf", "CormorantGaramond.ttf", "CormorantGaramond-Italic.ttf"})
        QFontDatabase::addApplicationFont(":/assets/fonts/" + f);

    QPalette pal;   // palette sombre pour les widgets non stylés (calendrier, boîtes de dialogue…)
    pal.setColor(QPalette::Window, QColor("#161616")); pal.setColor(QPalette::WindowText, QColor("#ece8dd"));
    pal.setColor(QPalette::Base, QColor("#232323")); pal.setColor(QPalette::AlternateBase, QColor("#1e1e1e"));
    pal.setColor(QPalette::Text, QColor("#ece8dd")); pal.setColor(QPalette::Button, QColor("#262626"));
    pal.setColor(QPalette::ButtonText, QColor("#ece8dd")); pal.setColor(QPalette::Highlight, QColor("#c8a85c"));
    pal.setColor(QPalette::HighlightedText, QColor("#1a1a1a")); pal.setColor(QPalette::ToolTipBase, QColor("#2a2a2a"));
    pal.setColor(QPalette::ToolTipText, QColor("#ece8dd"));
    app.setPalette(pal);

    QFile theme(":/theme.qss");
    if (theme.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(theme.readAll()));

    QCommandLineParser parser;
    parser.setApplicationDescription("AUREA — Gestion de la production");
    parser.addHelpOption();
    parser.addOption({"database", "Chemin de la base SQLite (ex. celle du module employés).", "path"});
    parser.addOption({"empty", "Ne pas charger les tâches d'exemple."});
    parser.addOption({"render", "Capturer les cinq pages puis quitter.", "directory"});
    parser.process(app);

    QString path = parser.value("database");
    if (path.isEmpty()) path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/production.sqlite";
    Database database;
    if (!database.open(path)) {
        QMessageBox::critical(nullptr, "AUREA", database.error());
        return 1;
    }
    MainWindow window(database.database(), !parser.isSet("empty"));
    if (QScreen *screen = app.primaryScreen()) {
        const QSize avail = screen->availableGeometry().size() - QSize(40, 70);
        window.resize(std::min(1500, avail.width()), std::min(920, avail.height()));
    }
    window.show();
    if (parser.isSet("render"))
        QTimer::singleShot(300, &app, [&] { window.renderScreenshots(parser.value("render")); app.quit(); });
    return app.exec();
}
