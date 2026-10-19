#include "productionkit.h"
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace Kit {
static void draw(QPainter &p, Icon k, const QColor &c) {
    QPen pen(c, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen); p.setBrush(Qt::NoBrush);
    switch (k) {
    case Icon::Grid:
        for (QPointF o : {QPointF(3, 3), QPointF(14, 3), QPointF(3, 14), QPointF(14, 14)})
            p.drawRoundedRect(QRectF(o.x(), o.y(), 7, 7), 1.8, 1.8);
        break;
    case Icon::Tasks:
        p.drawRoundedRect(QRectF(5, 4.5, 14, 16.5), 2.2, 2.2);
        p.drawRoundedRect(QRectF(9, 2.5, 6, 4), 1.2, 1.2);
        p.drawPolyline(QPolygonF({{8.5, 13.5}, {11, 16}, {15.5, 11}}));
        break;
    case Icon::Calendar:
        p.drawRoundedRect(QRectF(3.5, 5, 17, 15.5), 2.2, 2.2);
        p.drawLine(QPointF(3.5, 10), QPointF(20.5, 10));
        p.drawLine(QPointF(8, 3), QPointF(8, 7)); p.drawLine(QPointF(16, 3), QPointF(16, 7));
        break;
    case Icon::Trend:
        p.drawPolyline(QPolygonF({{3, 17}, {9, 11}, {13, 15}, {21, 6}}));
        p.drawPolyline(QPolygonF({{15, 6}, {21, 6}, {21, 12}}));
        break;
    case Icon::Chart:
        p.drawLine(QPointF(6, 20), QPointF(6, 12)); p.drawLine(QPointF(12, 20), QPointF(12, 5));
        p.drawLine(QPointF(18, 20), QPointF(18, 9));
        break;
    case Icon::Help: {
        p.drawEllipse(QRectF(3, 3, 18, 18));
        QFont f = p.font(); f.setPixelSize(12); f.setBold(true); p.setFont(f);
        p.drawText(QRectF(3, 3, 18, 18), Qt::AlignCenter, QStringLiteral("?"));
        break;
    }
    case Icon::Search:
        p.drawEllipse(QRectF(4, 4, 12, 12)); p.drawLine(QPointF(14, 14), QPointF(20, 20));
        break;
    case Icon::Alert:
        p.drawPolygon(QPolygonF({{12, 3.5}, {21.5, 19.5}, {2.5, 19.5}}));
        p.drawLine(QPointF(12, 9.5), QPointF(12, 14)); p.drawPoint(QPointF(12, 16.8));
        break;
    case Icon::Clock:
        p.drawEllipse(QRectF(3, 3, 18, 18));
        p.drawPolyline(QPolygonF({{12, 7}, {12, 12}, {15.5, 14}}));
        break;
    case Icon::Check:
        p.drawEllipse(QRectF(3, 3, 18, 18));
        p.drawPolyline(QPolygonF({{8, 12.5}, {11, 15.5}, {16, 9}}));
        break;
    case Icon::List:
        for (int y : {7, 12, 17}) { p.drawLine(QPointF(8.5, y), QPointF(20, y)); p.drawPoint(QPointF(4.5, y)); }
        break;
    case Icon::Chevron:
        p.drawPolyline(QPolygonF({{9, 6}, {15, 12}, {9, 18}}));
        break;
    }
}
QPixmap pixmap(Icon icon, int px, const QColor &color) {
    const qreal dpr = 2.0;
    QPixmap pm(int(px * dpr), int(px * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(px / 24.0, px / 24.0);
    draw(p, icon, color);
    return pm;
}
QIcon navIcon(Icon icon, const QColor &normal, const QColor &active) {
    QIcon ic;
    ic.addPixmap(pixmap(icon, 20, normal), QIcon::Normal, QIcon::Off);
    ic.addPixmap(pixmap(icon, 20, active), QIcon::Normal, QIcon::On);
    return ic;
}
}

// ------------------------------------------------------------------ courbe
void AreaChart::setData(const QVector<double> &values, const QStringList &labels, const QString &unit) {
    vals = values; labs = labels; suffix = unit; update();
}
void AreaChart::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF plot(52, 10, width() - 52 - 18, height() - 10 - 30);
    double maxV = 0;
    for (double v : vals) maxV = std::max(maxV, v);
    double step = 1;
    if (maxV > 4) {
        const double raw = maxV / 4.0, mag = std::pow(10.0, std::floor(std::log10(raw))), n = raw / mag;
        step = (n <= 1 ? 1 : n <= 2 ? 2 : n <= 5 ? 5 : 10) * mag;
    }
    const double top = step * 4;
    QFont f = font(); f.setPointSizeF(8.5); p.setFont(f);
    for (int i = 0; i <= 4; ++i) {
        const double y = plot.bottom() - plot.height() * i / 4.0;
        p.setPen(QPen(i == 0 ? QColor("#3a3a3f") : Kit::line, 1));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(Kit::muted);
        p.drawText(QRectF(0, y - 10, plot.left() - 8, 20), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(step * i, 'f', 0) + suffix);
    }
    const int n = vals.size();
    if (n == 0) return;
    QVector<QPointF> pts;
    for (int i = 0; i < n; ++i) {
        const double x = n == 1 ? plot.center().x() : plot.left() + plot.width() * i / (n - 1);
        pts.append(QPointF(x, plot.bottom() - plot.height() * std::min(vals[i], top) / top));
        p.setPen(Kit::muted);
        if (i < labs.size()) p.drawText(QRectF(x - 30, plot.bottom() + 6, 60, 18), Qt::AlignCenter, labs[i]);
    }
    QPainterPath path(pts[0]);
    for (int i = 0; i + 1 < n; ++i) {
        const QPointF p0 = pts[std::max(0, i - 1)], p1 = pts[i], p2 = pts[i + 1], p3 = pts[std::min(n - 1, i + 2)];
        QPointF c1 = p1 + (p2 - p0) / 6.0, c2 = p2 - (p3 - p1) / 6.0;
        c1.setY(std::min(c1.y(), plot.bottom())); c2.setY(std::min(c2.y(), plot.bottom()));
        path.cubicTo(c1, c2, p2);
    }
    QPainterPath fill = path;
    fill.lineTo(pts.last().x(), plot.bottom()); fill.lineTo(pts.first().x(), plot.bottom()); fill.closeSubpath();
    QLinearGradient g(0, plot.top(), 0, plot.bottom());
    g.setColorAt(0, QColor(227, 210, 154, 95)); g.setColorAt(1, QColor(227, 210, 154, 8));
    p.fillPath(fill, g);
    p.setPen(QPen(QColor("#f1e3b2"), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    p.setPen(QPen(Kit::gold2, 2)); p.setBrush(QColor("#f1e3b2"));
    p.drawEllipse(pts.last(), 4, 4);
}

// ------------------------------------------------------------------ barres
void BarChart::setData(const QVector<QPair<QString, double>> &rows, const QString &unit, int decimals) {
    data = rows; suffix = unit; dec = decimals;
    setMinimumHeight(std::max(60, int(rows.size()) * 38 + 8));
    update();
}
void BarChart::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (data.isEmpty()) {
        p.setPen(Kit::muted); p.drawText(rect(), Qt::AlignCenter, QStringLiteral("Aucune donnée"));
        return;
    }
    double maxV = 0;
    for (const auto &r : data) maxV = std::max(maxV, r.second);
    if (maxV <= 0) maxV = 1;
    const int labelW = std::min(230, width() * 40 / 100), valueW = 64;
    int y = 4;
    for (const auto &r : data) {
        p.setPen(Kit::text);
        p.drawText(QRect(0, y, labelW - 10, 30), Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(r.first, Qt::ElideRight, labelW - 10));
        const QRectF trackR(labelW, y + 10, width() - labelW - valueW, 10);
        p.setPen(Qt::NoPen); p.setBrush(Kit::track);
        p.drawRoundedRect(trackR, 5, 5);
        QRectF fillR = trackR; fillR.setWidth(std::max(r.second > 0 ? 10.0 : 0.0, trackR.width() * r.second / maxV));
        QLinearGradient g(fillR.topLeft(), fillR.topRight());
        g.setColorAt(0, QColor("#c9b27c")); g.setColorAt(1, QColor("#e9d8a8"));
        p.setBrush(g);
        p.drawRoundedRect(fillR, 5, 5);
        p.setPen(Kit::text);
        p.drawText(QRectF(trackR.right() + 8, y, valueW - 8, 30), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(r.second, 'f', dec) + suffix);
        y += 38;
    }
}
