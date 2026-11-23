#include "spectacle.h"
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace {
const double Pi = 3.14159265358979;
double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }
double smooth(double v)
{
    v = clamp01(v);
    return v * v * v * (v * (v * 6 - 15) + 10);
}
double lerp(double a, double b, double t) { return a + (b - a) * t; }
}

Spectacle::Point Spectacle::turn(Point p, double ax, double ay, double az) const
{
    double c = std::cos(ax), s = std::sin(ax);
    p = {p.x, p.y * c - p.z * s, p.y * s + p.z * c};
    c = std::cos(ay);
    s = std::sin(ay);
    p = {p.x * c + p.z * s, p.y, -p.x * s + p.z * c};
    c = std::cos(az);
    s = std::sin(az);
    return {p.x * c - p.y * s, p.x * s + p.y * c, p.z};
}

void Spectacle::segment(const Point &a, const Point &b, double alpha, int color)
{
    if (alpha < 0.02) return;
    const double focal = m_reach * 1.5, za = focal + a.z, zb = focal + b.z;
    if (za < focal * 0.18 || zb < focal * 0.18) return;
    const QLineF line(m_center.x() + a.x * focal / za, m_center.y() + a.y * focal / za, m_center.x() + b.x * focal / zb, m_center.y() + b.y * focal / zb);
    if (std::max(line.x1(), line.x2()) < 0 || std::min(line.x1(), line.x2()) > m_area.width() || std::max(line.y1(), line.y2()) < 0 || std::min(line.y1(), line.y2()) > m_area.height()) return;
    const double z = (a.z + b.z) / 2, near = clamp01(0.5 - z / (m_reach * 1.3));
    const int depth = std::min(Depths - 1, int(near * Depths)), level = std::min(Alphas - 1, int(clamp01(alpha) * Alphas));
    m_lines[z < 0 ? 1 : 0][color][depth][level].append(line);
}

void Spectacle::orbit(double radius, double tilt, double roll, double start, double sweep, double alpha, int color)
{
    if (sweep < 0.02) return;
    const int parts = 84;
    Point last{};
    for (int k = 0; k <= parts; ++k) {
        const double s = k / double(parts), a = start + sweep * s;
        const Point p = turn({std::cos(a) * radius, std::sin(a) * radius, 0}, tilt, 0, roll);
        if (k) segment(last, p, alpha * std::min(1.0, 0.12 + s * 1.1), color);
        last = p;
    }
}

void Spectacle::silk(const Frame &f, bool forward)
{
    const int parts = 90;
    const double head = smooth(f.arrive), tail = smooth(f.leave), width = f.area.width() * 0.56;
    for (int i = 0; i < 3; ++i) {
        const double lag = i * 0.06, from = clamp01(tail * 1.14 - lag), to = clamp01(head * 1.14 - lag);
        if (to - from < 0.004) continue;
        Point last{};
        for (int k = 0; k <= parts; ++k) {
            const double s = lerp(from, to, k / double(parts)), u = forward ? s : 1 - s, x = (u * 2 - 1) * width;
            const double hug = std::exp(-std::pow(x / (f.logo * 1.5), 2)), phase = x / (f.logo * 1.35) + f.time * 0.9 + i * 0.55;
            const Point p{x, std::sin(phase) * f.logo * (0.22 + 0.36 * hug) + (i - 1) * f.logo * 0.10, std::cos(phase) * f.logo * (0.7 + 0.8 * hug)};
            if (k) segment(last, p, std::sin(Pi * clamp01(u)) * std::min(1.0, (to - s) * 9 + 0.2) * (0.9 - 0.22 * i), i == 0 ? 1 : 0);
            last = p;
        }
    }
}

void Spectacle::breath(const Frame &f)
{
    const int lines = 5, parts = 40;
    const double head = smooth(f.arrive), tail = smooth(f.leave);
    for (int i = 0; i < lines; ++i) {
        const double lag = (i * 3 % lines) / double(lines) * 0.3, from = clamp01(tail * 1.3 - lag), to = clamp01(head * 1.3 - lag);
        if (to - from < 0.004) continue;
        const double angle = Pi * (0.12 + 0.76 * i / (lines - 1.0)), far = m_reach * 0.62;
        const double ex = std::cos(angle) * far * 1.25, ey = std::sin(angle) * far * 0.8;
        Point last{};
        for (int k = 0; k <= parts; ++k) {
            const double s = lerp(from, to, k / double(parts)), q = 1 - s;
            const Point p{ex * q * q + std::cos(angle) * f.logo * 0.52, ey * q + std::sin(angle) * f.logo * 0.52, std::sin(s * Pi + i) * f.logo * 0.8 * q};
            if (k) segment(last, p, std::min(1.0, (s - from) * 5) * (0.25 + 0.6 * s), i % 3 == 0 ? 1 : 0);
            last = p;
        }
    }
}

void Spectacle::build(const Frame &f)
{
    for (auto &side : m_lines)
        for (auto &color : side)
            for (auto &depth : color)
                for (auto &level : depth) level.resize(0);
    m_center = f.center;
    m_area = f.area;
    m_reach = std::hypot(f.area.width(), f.area.height()) / 2;
    const double in = smooth(f.arrive), out = smooth(f.leave), show = clamp01(f.arrive * 2.5) * (1 - smooth(f.leave)), t = f.time;
    if (f.mode == EmployeeIn || f.mode == EmployeeOut) {
        const bool entering = f.mode == EmployeeIn;
        const double way = entering ? 1 : -1;
        for (int i = 0; i < 2; ++i) {
            const double radius = f.logo * (1.02 + 0.34 * i) * (entering ? lerp(1.35, 1, in) * lerp(1, 0.75, out) : lerp(0.75, 1, in) * lerp(1, 1.5, out));
            orbit(radius, 1.18 + 0.06 * std::sin(t), (i ? -0.5 : 0.42) * way, way * t * (1.25 - 0.3 * i) + i * Pi, way * lerp(0.1, 4.6, in) * way, show * (0.95 - 0.25 * i), i ? 0 : 1);
        }
    } else if (f.mode == AdminIn || f.mode == AdminOut) {
        silk(f, f.mode == AdminIn);
    } else {
        breath(f);
    }
}

void Spectacle::paint(QPainter &p, bool front) const
{
    static const QColor colors[Colors] = {QColor(226, 207, 159), QColor(255, 247, 224), QColor(214, 158, 104)};
    p.save();
    p.setBrush(Qt::NoBrush);
    p.setOpacity(1);
    for (int depth = 0; depth < Depths; ++depth)
        for (int color = 0; color < Colors; ++color)
            for (int level = 0; level < Alphas; ++level) {
                const QVector<QLineF> &lines = m_lines[front ? 1 : 0][color][depth][level];
                if (lines.isEmpty()) continue;
                const double near = (depth + 0.5) / Depths, alpha = (level + 0.7) / Alphas;
                QColor c = colors[color];
                QPen pen(c, 1, Qt::SolidLine, Qt::FlatCap);
                pen.setCosmetic(true);
                const auto pass = [&](double dx, double dy, double strength) {
                    c.setAlphaF(clamp01(alpha * strength));
                    pen.setColor(c);
                    p.setPen(pen);
                    p.translate(dx, dy);
                    p.drawLines(lines);
                    p.translate(-dx, -dy);
                };
                pass(0, 0, 0.2 + 0.78 * near);
                if (depth >= 3) {
                    pass(0.7, 0.4, 0.34);
                }
            }
    p.restore();
}
