#pragma once
#include <QLineF>
#include <QPointF>
#include <QSizeF>
#include <QVector>

class QPainter;

class Spectacle {
public:
    enum Mode { EmployeeIn, EmployeeOut, AdminIn, AdminOut, Closing };
    struct Frame {
        Mode mode;
        QPointF center;
        double logo, time, arrive, leave;
        QSizeF area;
    };
    void build(const Frame &frame);
    void paint(QPainter &painter, bool front) const;

private:
    struct Point { double x, y, z; };
    Point turn(Point p, double ax, double ay, double az) const;
    void segment(const Point &a, const Point &b, double alpha, int color);
    void orbit(double radius, double tilt, double roll, double start, double sweep, double alpha, int color);
    void silk(const Frame &f, bool forward);
    void breath(const Frame &f);

    static constexpr int Colors = 3, Depths = 5, Alphas = 5;
    QVector<QLineF> m_lines[2][Colors][Depths][Alphas];
    QPointF m_center;
    QSizeF m_area;
    double m_reach = 1;
};
