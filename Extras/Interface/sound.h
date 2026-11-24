#pragma once
#include <QFile>
#include <QHash>
#include <QString>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Sound {
void prepare();
void play(const QString &name);
}
