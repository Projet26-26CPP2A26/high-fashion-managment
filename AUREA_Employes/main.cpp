#include "appsetup.h"
#include "database.h"
#include "employeemodule.h"
#include "employeewindow.h"
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
    const QString dataDir = setupAurea(app, QStringLiteral("AUREA — Gestion des employés"));

    Database employeeDatabase;
    if (!employeeDatabase.open(QDir(dataDir).filePath("employes.sqlite"))) {
        QMessageBox::critical(nullptr, "AUREA", employeeDatabase.error());
        return 1;
    }
    auto *window = new EmployeeWindow(employeeDatabase.database(), true);   // base vide : exemples chargés une seule fois
    if (!window->ready()) {
        QMessageBox::critical(nullptr, "AUREA", window->module()->repository().error());
        return 1;
    }
    window->setWindowTitle(QStringLiteral("AUREA — Gestion des employés"));

    // Disposition d'origine : colonne de boutons verticale à gauche, barre du haut, page.
    if (QScreen *screen = app.primaryScreen()) {
        const QSize available = screen->availableGeometry().size() - QSize(40, 70);
        window->resize(std::min(1480, available.width()), std::min(940, available.height()));
    }
    window->show();
    return app.exec();
}
