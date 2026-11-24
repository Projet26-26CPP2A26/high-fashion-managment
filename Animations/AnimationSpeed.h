#pragma once
#include <QElapsedTimer>
#include <algorithm>

namespace Motion {
constexpr int Quick = 160;
constexpr int Base = 240;
constexpr int Slow = 420;
constexpr int Step = 50;
constexpr int Glow = 1400;

inline double ease(double t)
{
    t = 1 - std::clamp(t, 0.0, 1.0);
    return 1 - t * t * t;
}

class Tween {
public:
    void go(double to, int ms)
    {
        m_from = value();
        m_to = to;
        m_ms = ms;
        m_clock.start();
    }
    void jump(double to)
    {
        m_from = m_to = to;
        m_clock.invalidate();
    }
    double value() const
    {
        return m_clock.isValid() ? m_from + (m_to - m_from) * ease(m_clock.elapsed() / double(m_ms)) : m_to;
    }
    double target() const
    {
        return m_to;
    }
    bool running() const
    {
        return m_clock.isValid() && m_clock.elapsed() < m_ms;
    }

private:
    double m_from = 0, m_to = 0;
    int m_ms = Base;
    QElapsedTimer m_clock;
};
}
