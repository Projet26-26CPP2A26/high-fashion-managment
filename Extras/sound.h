#pragma once
#include <QFile>
#include <QHash>
#include <QString>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

namespace Sound {
inline void play(const QString &name)
{
#ifdef Q_OS_WIN
    static QHash<QString, QByteArray> sounds;
    if (!sounds.contains(name)) {
        QFile file(":/assets/sounds/" + name + ".wav");
        sounds.insert(name, file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray());
    }
    const QByteArray &data = sounds[name];
    if (!data.isEmpty()) PlaySoundW(reinterpret_cast<LPCWSTR>(data.constData()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#else
    Q_UNUSED(name);
#endif
}
}
