#pragma once
#include <QApplication>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QLocale>
#include <QStandardPaths>

// Réglages communs : mêmes polices, même thème et mêmes dossiers de données que l'application intégrée.
inline QString setupAurea(QApplication &app, const QString &displayName)
{
    app.setApplicationName(QStringLiteral("AUREA"));
    app.setOrganizationName(QStringLiteral("Velora"));
    app.setApplicationDisplayName(displayName);
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/logo_aurea.png")));
    app.setStyle(QStringLiteral("Fusion"));
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
    for (const QString &file : QStringList{QStringLiteral("segoeui.ttf"), QStringLiteral("segoeuib.ttf"), QStringLiteral("georgia.ttf")})
        QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts/" + file));
    for (const QString &file : QStringList{QStringLiteral("Inter.ttf"), QStringLiteral("CormorantGaramond.ttf"), QStringLiteral("CormorantGaramond-Italic.ttf")})
        QFontDatabase::addApplicationFont(QStringLiteral(":/assets/fonts/") + file);
    app.setFont(QFont(QStringLiteral("Inter"), 10));
    app.setStyleSheet(QStringLiteral("QToolTip { background: #eee2bf; color: #23292b; border: none; padding: 7px; }"));
    // Même dossier que l'application intégrée : les données sont partagées.
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataDir);
    return dataDir;
}
