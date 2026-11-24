#pragma once
#include <QLineF>
#include <QPointF>
#include <QSizeF>
#include <QVector>

class QPainter;

class Spectacle {
public:
    enum Mode { EmployeeIn, EmployeeOut, AdminIn, AdminOut, Closing };
    class Frame {
    public:
        Mode mode = EmployeeIn;
        QPointF center;
        double logo = 0, time = 0, arrive = 0, leave = 0;
        QSizeF area;
    };
    void build(const Frame &frame);
    void paint(QPainter &painter, bool front) const;

private:
    class Point {
    public:
        double x = 0, y = 0, z = 0;
    };
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
