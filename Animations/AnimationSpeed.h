#pragma once
#include <QScreen>
#include <QWidget>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

// Durées (ms) et courbes communes à toutes les animations.
namespace Motion {
// Windows réveille les minuteries toutes les 15 ms par défaut (64 images/s) : on demande 1 ms.
struct SmoothTimers {
#ifdef Q_OS_WIN
    SmoothTimers() { timeBeginPeriod(1); }
    ~SmoothTimers() { timeEndPeriod(1); }
#endif
};
constexpr int Fast = 180;
constexpr int Page = 240;
constexpr int Normal = 320;
constexpr int Long = 700;
constexpr double Pi = 3.14159265358979;

// Une image par rafraîchissement de l'écran (60, 120, 144 Hz...).
inline int frame(const QWidget *w)
{
    const double hz = w->screen() ? w->screen()->refreshRate() : 60;
    return std::clamp(int(1000 / std::max(30.0, hz)), 6, 33);
}
inline double part(double t, double from, double to) { return std::clamp((t - from) / (to - from), 0.0, 1.0); }
inline double soft(double v) { v = std::clamp(v, 0.0, 1.0); return 1 - std::pow(1 - v, 3); }
inline double both(double v) { v = std::clamp(v, 0.0, 1.0); return v * v * v * (v * (6 * v - 15) + 10); }
inline double spring(double v) { v = std::clamp(v, 0.0, 1.0); return 1 + 2.1 * std::pow(v - 1, 3) + 1.1 * std::pow(v - 1, 2); }
inline double mix(double a, double b, double t) { return a + (b - a) * t; }
}
