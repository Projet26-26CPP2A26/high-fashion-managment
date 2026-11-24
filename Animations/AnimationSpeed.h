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

class Motion
{
public:
    class SmoothTimers
    {
    public:
#ifdef Q_OS_WIN
        SmoothTimers()
        {
            timeBeginPeriod(1);
        }
        ~SmoothTimers()
        {
            timeEndPeriod(1);
        }
#endif
    };
    static constexpr int Fast = 180;
    static constexpr int Page = 240;
    static constexpr int Normal = 320;
    static constexpr int Long = 700;
    static constexpr double Pi = 3.14159265358979;

    static int frame(const QWidget *w)
    {
        const double hz = w->screen() ? w->screen()->refreshRate() : 60;
        return std::clamp(int(1000 / std::max(30.0, hz)), 6, 33);
    }
    static double part(double t, double from, double to)
    {
        return std::clamp((t - from) / (to - from), 0.0, 1.0);
    }
    static double soft(double v)
    {
        v = std::clamp(v, 0.0, 1.0);
        return 1 - std::pow(1 - v, 3);
    }
    static double both(double v)
    {
        v = std::clamp(v, 0.0, 1.0);
        return v * v * v * (v * (6 * v - 15) + 10);
    }
    static double spring(double v)
    {
        v = std::clamp(v, 0.0, 1.0);
        return 1 + 2.1 * std::pow(v - 1, 3) + 1.1 * std::pow(v - 1, 2);
    }
    static double mix(double a, double b, double t)
    {
        return a + (b - a) * t;
    }
};
