#include "oracleconnection.h"
#include "materialwindow.h"
#include <QApplication>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QLocale>
#include <QMessageBox>
#include <QScreen>
#include <QStringList>
#include <algorithm>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("AUREA");
    app.setOrganizationName("Velora");
    app.setApplicationDisplayName("AUREA — Gestion des matières");
    app.setWindowIcon(QIcon(":/assets/logo_aurea.png"));
    app.setStyle("Fusion");
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));

    const QString windowsFonts = QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts/");
    const QStringList systemFonts = {"segoeui.ttf", "segoeuib.ttf", "georgia.ttf"};
    for (const QString &file : systemFonts)
        QFontDatabase::addApplicationFont(windowsFonts + file);
    const QStringList appFonts = {"Inter.ttf", "CormorantGaramond.ttf", "CormorantGaramond-Italic.ttf"};
    for (const QString &file : appFonts)
        QFontDatabase::addApplicationFont(":/assets/fonts/" + file);
    app.setFont(QFont("Inter", 10));
    app.setStyleSheet("QToolTip { background: #eee2bf; color: #23292b; border: none; padding: 7px; }");

    if (!OracleConnection::instance().open()) {
        QMessageBox::critical(nullptr, "AUREA — Oracle", OracleConnection::instance().errorMessage());
        return 1;
    }

    int result = 0;
    {
        MaterialWindow window;
        QScreen *screen = app.primaryScreen();
        if (screen) {
            const QSize available = screen->availableGeometry().size() - QSize(40, 70);
            window.resize(std::min(1480, available.width()), std::min(940, available.height()));
        }
        window.show();
        window.playOpening();
        result = app.exec();
    }
    OracleConnection::instance().close();
    return result;
}
