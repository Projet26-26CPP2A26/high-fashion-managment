#include "LinesAnimation.h"
#include "AnimationSpeed.h"
#include <QPainter>

void LinesAnimation::paint(QPainter &p, const Scene &s, bool front)
{
    if (s.alpha < 0.01 || s.drawn < 0.001)
        return;
    const int threads = 9, parts = 72;
    const double w = s.area.width() * 0.62, gather = Motion::both(s.gather), t = s.time;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < threads; ++i) {
        const double k = i / double(threads - 1) - 0.5, phase = i * 1.37;
        const bool bright = i % 3 == 1;
        const double head = std::clamp(s.drawn * 1.35 - std::abs(k) * 0.35, 0.0, 1.0);
        QPointF last;
        double lastDepth = 0;
        for (int j = 0; j <= parts; ++j) {
            const double u = j / double(parts) * head;
            // fil libre
            const double x = (u * 2 - 1) * w;
            const double bend = std::exp(-std::pow(x / (s.radius * 2.6), 2));
            const double y = std::sin(x / (s.radius * 1.9) + t * 0.8 + phase) * s.radius * (0.35 + 0.45 * bend) + k * s.radius * (1.9 - 1.2 * bend);
            const double z = std::cos(x / (s.radius * 1.5) + t * 0.9 + phase);
            // anneau incliné
            const double a = u * 2 * Motion::Pi * 0.94 + t * (0.7 + 0.12 * i) * (i % 2 ? -1 : 1) + phase;
            const double r = s.radius * (1.04 + 0.055 * i), tilt = 0.30 + 0.16 * std::sin(phase + t * 0.35), roll = k * 1.7 + 0.2 * std::sin(t * 0.5 + i);
            const double ox = std::cos(a) * r, oy = std::sin(a) * r * tilt, oz = std::sin(a);
            const double rx = ox * std::cos(roll) - oy * std::sin(roll), ry = ox * std::sin(roll) + oy * std::cos(roll);
            const double depth = Motion::mix(z, oz, gather), zoom = 1 + 0.10 * depth;
            const QPointF at = s.center + QPointF(Motion::mix(x, rx, gather), Motion::mix(y, ry, gather)) * zoom;
            const bool inFront = depth + lastDepth > 0;
            if (j && inFront == front) {
                const double near = 0.5 + 0.5 * std::clamp((depth + lastDepth) / 2, -1.0, 1.0);
                const double fade = Motion::mix(std::sin(Motion::Pi * std::clamp((x / w + 1) / 2, 0.0, 1.0)), 0.25 + 0.75 * u, gather);
                QColor c(bright ? QColor(255, 246, 220) : i % 3 == 2 ? QColor(214, 170, 120) : QColor(226, 207, 159));
                const double alpha = s.alpha * fade * (0.16 + 0.74 * near * near);
                if (alpha > 0.015) {
                    if (near > 0.72) {
                        c.setAlphaF(alpha * 0.16);
                        p.setPen(QPen(c, 5, Qt::SolidLine, Qt::FlatCap));
                        p.drawLine(last, at);
                    }
                    c.setAlphaF(alpha);
                    p.setPen(QPen(c, 0.7 + 0.9 * near, Qt::SolidLine, Qt::FlatCap));
                    p.drawLine(last, at);
                }
            }
            last = at;
            lastDepth = depth;
        }
    }
    p.restore();
}
