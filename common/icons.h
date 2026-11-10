#pragma once
// Icônes « trait fin » dessinées avec QPainter : aucune image à embarquer.
// Elles prennent la couleur demandée (clair au repos, foncé sur le bouton doré actif).
// Chaque icône est dessinée dans un carré de 24 x 24.
#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRectF>
#include <QScreen>

namespace Icons {
enum Kind { Grid, People, Clock, Chart, Help, Search, Alert, Document };

inline QPixmap pixmap(Kind kind, const QColor &color, int size = 20)
{
    const qreal ratio = qApp && qApp->primaryScreen() ? qApp->primaryScreen()->devicePixelRatio() : 1.0;
    QPixmap image(int(size * ratio), int(size * ratio));
    image.setDevicePixelRatio(ratio);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 24.0, size / 24.0);
    QPen pen(color, 1.7);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    switch (kind) {
    case Grid:
        p.drawRoundedRect(QRectF(3.5, 3.5, 7, 7), 1.8, 1.8);
        p.drawRoundedRect(QRectF(13.5, 3.5, 7, 7), 1.8, 1.8);
        p.drawRoundedRect(QRectF(3.5, 13.5, 7, 7), 1.8, 1.8);
        p.drawRoundedRect(QRectF(13.5, 13.5, 7, 7), 1.8, 1.8);
        break;
    case People:
        p.drawEllipse(QPointF(9, 8), 3.4, 3.4);
        p.drawArc(QRectF(2.5, 14, 13, 13), 0, 180 * 16);
        p.drawEllipse(QPointF(17, 9), 2.5, 2.5);
        p.drawArc(QRectF(15, 14.5, 7, 7), 0, 180 * 16);
        break;
    case Clock: {
        p.drawEllipse(QPointF(12, 12), 8.8, 8.8);
        QPainterPath h;
        h.moveTo(12, 7);
        h.lineTo(12, 12);
        h.lineTo(15.6, 14.2);
        p.drawPath(h);
        break;
    }
    case Chart:
        p.drawLine(QPointF(6, 20), QPointF(6, 13));
        p.drawLine(QPointF(12, 20), QPointF(12, 8));
        p.drawLine(QPointF(18, 20), QPointF(18, 4));
        break;
    case Help: {
        p.drawEllipse(QPointF(12, 12), 8.8, 8.8);
        QPainterPath q;
        q.moveTo(9.4, 9.8);
        q.cubicTo(9.6, 6.9, 14.6, 6.9, 14.6, 9.9);
        q.cubicTo(14.6, 12, 12, 12, 12, 14.3);
        p.drawPath(q);
        p.drawPoint(QPointF(12, 17.2));
        break;
    }
    case Search:
        p.drawEllipse(QPointF(10.5, 10.5), 6.3, 6.3);
        p.drawLine(QPointF(15.2, 15.2), QPointF(20.5, 20.5));
        break;
    case Alert: {
        QPainterPath t;
        t.moveTo(12, 3.6);
        t.lineTo(21.2, 19.6);
        t.lineTo(2.8, 19.6);
        t.closeSubpath();
        p.drawPath(t);
        p.drawLine(QPointF(12, 9.5), QPointF(12, 14));
        p.drawPoint(QPointF(12, 16.9));
        break;
    }
    case Document: {   // feuille au coin plié
        QPainterPath d;
        d.moveTo(6.5, 3.5);
        d.lineTo(14, 3.5);
        d.lineTo(19, 8.5);
        d.lineTo(19, 20.5);
        d.lineTo(6.5, 20.5);
        d.closeSubpath();
        p.drawPath(d);
        QPainterPath fold;
        fold.moveTo(14, 3.5);
        fold.lineTo(14, 8.5);
        fold.lineTo(19, 8.5);
        p.drawPath(fold);
        p.drawLine(QPointF(9.5, 13), QPointF(15.5, 13));
        p.drawLine(QPointF(9.5, 16.6), QPointF(15.5, 16.6));
        break;
    }
    }
    return image;
}

// Icône de navigation : claire au repos, foncée lorsque le bouton doré est actif.
inline QIcon nav(Kind kind, const QColor &rest = QColor("#b9b4a8"), const QColor &active = QColor("#1c1c1e"))
{
    QIcon icon;
    icon.addPixmap(pixmap(kind, rest), QIcon::Normal, QIcon::Off);
    icon.addPixmap(pixmap(kind, active), QIcon::Normal, QIcon::On);
    return icon;
}
}
