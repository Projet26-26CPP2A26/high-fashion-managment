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

class Sound
{
public:
    static void play(const QString &name)
    {
#ifdef Q_OS_WIN
        static QHash<QString, QByteArray> loaded;
        if (!loaded.contains(name)) {
            QFile file(":/assets/sounds/" + name + ".wav");
            if (file.open(QIODevice::ReadOnly))
                loaded.insert(name, file.readAll());
        }
        if (loaded.contains(name))
            PlaySoundW(reinterpret_cast<LPCWSTR>(loaded[name].constData()), nullptr,
                       SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#else
        Q_UNUSED(name);
#endif
    }
};
