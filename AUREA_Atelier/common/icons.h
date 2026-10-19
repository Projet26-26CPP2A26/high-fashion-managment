#pragma once
// Icônes « trait fin » dessinées avec QPainter : aucune image à embarquer.
// Elles prennent la couleur demandée (clair au repos, foncé sur le bouton doré actif).
//
// Ce fichier regroupe les jeux d'icônes des modules Employés, Matières et Production,
// ainsi que celles de la barre de navigation principale.
#include <QApplication>
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRectF>
#include <QScreen>
#include <cmath>

namespace Icons {
enum Kind {
    // module Employés
    Grid, People, Assign, Clock, Chart, Help, Search, Check, Alert,
    // module Clients
    AddPerson, Ruler, Star, Calendar,
    // barre principale
    Badge,
    // module Commandes
    Document, Scissors
};

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
    case Assign: {
        p.drawRoundedRect(QRectF(5, 4.5, 14, 16.5), 2.2, 2.2);
        p.drawRoundedRect(QRectF(9, 2.5, 6, 4), 1.2, 1.2);
        QPainterPath c;
        c.moveTo(8.7, 13.3);
        c.lineTo(11, 15.6);
        c.lineTo(15.4, 10.8);
        p.drawPath(c);
        break;
    }
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
    case Check: {
        p.drawEllipse(QPointF(12, 12), 8.8, 8.8);
        QPainterPath c;
        c.moveTo(8, 12.4);
        c.lineTo(10.9, 15.2);
        c.lineTo(16, 9.4);
        p.drawPath(c);
        break;
    }
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
    case AddPerson:
        p.drawEllipse(QPointF(10, 8), 3.6, 3.6);
        p.drawArc(QRectF(3, 14.5, 14, 14), 0, 180 * 16);
        p.drawLine(QPointF(19, 6), QPointF(19, 12));
        p.drawLine(QPointF(16, 9), QPointF(22, 9));
        break;
    case Ruler: {
        p.save();
        p.translate(12, 12);
        p.rotate(-45);
        p.drawRoundedRect(QRectF(-10, -3.6, 20, 7.2), 1.6, 1.6);
        for (int i = -6; i <= 6; i += 3)
            p.drawLine(QPointF(i, -3.6), QPointF(i, i % 6 == 0 ? 0.2 : -1.2));
        p.restore();
        break;
    }
    case Star: {
        QPainterPath s;
        for (int i = 0; i < 10; ++i) {
            const double angle = -M_PI / 2 + i * M_PI / 5, radius = i % 2 ? 4.3 : 9.2;
            const QPointF pt(12 + radius * std::cos(angle), 12.6 + radius * std::sin(angle));
            if (i == 0) s.moveTo(pt); else s.lineTo(pt);
        }
        s.closeSubpath();
        p.drawPath(s);
        break;
    }
    case Calendar:
        p.drawRoundedRect(QRectF(3.5, 5, 17, 15.5), 2.5, 2.5);
        p.drawLine(QPointF(3.5, 10), QPointF(20.5, 10));
        p.drawLine(QPointF(8, 3), QPointF(8, 7));
        p.drawLine(QPointF(16, 3), QPointF(16, 7));
        break;
    case Badge:   // carte d'identité : employé
        p.drawRoundedRect(QRectF(3, 5, 18, 14), 2.6, 2.6);
        p.drawEllipse(QPointF(9, 10.6), 2.1, 2.1);
        p.drawArc(QRectF(5.4, 13.4, 7.2, 7.2), 0, 180 * 16);
        p.drawLine(QPointF(14.5, 10), QPointF(18, 10));
        p.drawLine(QPointF(14.5, 13.6), QPointF(18, 13.6));
        break;
    case Document: {   // feuille au coin plié : devis
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
    case Scissors:   // ciseaux : commandes d'atelier
        p.drawEllipse(QPointF(6.5, 17.5), 2.8, 2.8);
        p.drawEllipse(QPointF(17.5, 17.5), 2.8, 2.8);
        p.drawLine(QPointF(8.4, 15.4), QPointF(18.5, 3.5));
        p.drawLine(QPointF(15.6, 15.4), QPointF(5.5, 3.5));
        break;
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
