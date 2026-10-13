#include "appsetup.h"
#include "inventoryrepository.h"
#include "materialwindow.h"
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
    const QString dataDir = setupAurea(app, QStringLiteral("AUREA — Gestion des matières"));

    static InventoryRepository stockRepository;   // stock.sqlite : exemples chargés une seule fois si la base est vide
    if (!stockRepository.open(QDir(dataDir).filePath("stock.sqlite"), true)) {
        QMessageBox::critical(nullptr, "AUREA", stockRepository.error());
        return 1;
    }
    auto *window = new MaterialWindow(stockRepository);
    window->setWindowTitle(QStringLiteral("AUREA — Gestion des matières"));

    // Disposition d'origine : colonne de boutons verticale à gauche, barre du haut, page.
    if (QScreen *screen = app.primaryScreen()) {
        const QSize available = screen->availableGeometry().size() - QSize(40, 70);
        window->resize(std::min(1480, available.width()), std::min(940, available.height()));
    }
    window->show();
    return app.exec();
}
