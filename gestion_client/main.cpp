#include "appsetup.h"
#include "clientdatabase.h"
#include "clientwindow.h"
#include <QTimer>
#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QScreen>
#include <algorithm>

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    if (qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR"))
        qputenv("QT_QPA_FONTDIR", QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts").toUtf8());
#endif
    QApplication app(argc, argv);
    const QString dataDir = setupAurea(app, QStringLiteral("AUREA — Gestion des clients"));

    if (!openClientDatabase()) {
        QMessageBox::critical(nullptr, "AUREA", "Impossible d'ouvrir la base de données des clients.");
        return 1;
    }
    auto *window = new ClientWindow;
    window->setWindowTitle(QStringLiteral("AUREA — Gestion des clients"));

    // Disposition d'origine : colonne de boutons verticale à gauche, barre du haut, page.
    if (QScreen *screen = app.primaryScreen()) {
        const QSize available = screen->availableGeometry().size() - QSize(40, 70);
        window->resize(std::min(1480, available.width()), std::min(940, available.height()));
    }
    window->show();
    QTimer::singleShot(400, window, [window] { window->announceBirthdays(); });   // rappel des anniversaires du jour
    return app.exec();
}
