#pragma once

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
    Grid,
    People,
    Assign,
    Clock,
    Chart,
    Help,
    Search,
    Check,
    Alert,

    AddPerson,
    Ruler,
    Star,
    Calendar,

    Badge,

    Document,
    Scissors,

    Office,
    Online
};

QPixmap pixmap(Kind kind, const QColor &color, int size = 20);
QIcon nav(Kind kind, const QColor &rest = QColor("#b9b4a8"), const QColor &active = QColor("#1c1c1e"));
}
