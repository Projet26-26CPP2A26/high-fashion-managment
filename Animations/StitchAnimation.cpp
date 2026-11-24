#include "StitchAnimation.h"
#include "Motion.h"
#include <QEvent>
#include <QPainter>
#include <QPainterPath>

namespace {
const double OpenTime = 2500, CloseTime = 1700;
}

StitchAnimation::StitchAnimation(Mode mode, QWidget *window) : QWidget(window), m_mode(mode)
{
    setGeometry(window->rect());
    window->installEventFilter(this);
    const double ratio = devicePixelRatioF();
    m_logo = QPixmap(":/assets/logo_aurea.png").scaled(QSize(150, 150) * ratio, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_logo.setDevicePixelRatio(ratio);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        update();
        if (m_clock.elapsed() > (m_mode == Opening ? OpenTime : CloseTime))
            finish();
    });
    show();
    raise();
    m_clock.start();
    m_timer.start(Motion::Frame);
}

bool StitchAnimation::eventFilter(QObject *object, QEvent *event)
{
    if (object == parent() && event->type() == QEvent::Resize)
        setGeometry(parentWidget()->rect());
    return QWidget::eventFilter(object, event);
}

void StitchAnimation::finish()
{
    if (m_done)
        return;
    m_done = true;
    m_timer.stop();
    if (m_mode == Opening)
        hide();
    emit finished();
    deleteLater();
}

// Deux pans de tissu ; leur bord suit une courbe, comme une fermeture éclair qu'on descend.
void StitchAnimation::curtains(QPainter &p, double open)
{
    const double w = width(), h = height(), mid = w / 2;
    for (int side = -1; side <= 1; side += 2) {
        QPainterPath cloth;
        QPolygonF edge;
        for (int i = 0; i <= 48; ++i) {
            const double v = i / 48.0;
            const double local = Motion::both(std::clamp(open * 1.9 - v * 0.9, 0.0, 1.0));
            edge << QPointF(mid + side * local * (mid + 24), v * h);
        }
        cloth.addPolygon(edge);
        const double far = side < 0 ? -30 : w + 30;
        cloth.lineTo(far, h);
        cloth.lineTo(far, 0);
        cloth.closeSubpath();
        QLinearGradient fabric(mid, 0, side < 0 ? 0 : w, 0);
        fabric.setColorAt(0, QColor("#1b1b1d"));
        fabric.setColorAt(0.5, QColor("#202023"));
        fabric.setColorAt(1, QColor("#17171a"));
        p.setPen(Qt::NoPen);
        p.setBrush(fabric);
        p.drawPath(cloth);
        if (open > 0 && open < 1) {
            QPen teeth(QColor("#c9b27c"), 3, Qt::CustomDashLine, Qt::FlatCap);
            teeth.setDashPattern({2.2, 1.6});
            p.setPen(teeth);
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(edge);
        }
    }
}

void StitchAnimation::emblem(QPainter &p, double sewn, double words)
{
    const QPointF c(width() / 2.0, height() / 2.0 - 30);
    const double r = 104, pi = 3.14159265;
    const QSizeF size = m_logo.deviceIndependentSize();
    const double base = p.opacity();
    p.setOpacity(base * Motion::soft(sewn * 2));
    p.drawPixmap(c - QPointF(size.width() / 2, size.height() / 2), m_logo);
    p.setOpacity(base);
    const int count = 44;
    p.setPen(QPen(QColor("#e2cf9f"), 2.4, Qt::SolidLine, Qt::RoundCap));
    for (int i = 0; i < int(count * sewn); ++i) {
        const double a = -pi / 2 + 2 * pi * i / count, b = a + 2 * pi / count * 0.58;
        p.drawLine(c + QPointF(std::cos(a), std::sin(a)) * r, c + QPointF(std::cos(b), std::sin(b)) * r);
    }
    if (sewn > 0 && sewn < 1) {
        const double a = -pi / 2 + 2 * pi * sewn;
        const QPointF tip = c + QPointF(std::cos(a), std::sin(a)) * r;
        p.save();
        p.translate(tip);
        p.rotate(a * 180 / pi + 90 + 10 * std::sin(sewn * count * pi));
        p.setPen(QPen(QColor("#f7f0dc"), 3, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(0, 0), QPointF(-42, 0));
        p.setPen(QPen(QColor("#c9b27c"), 1.3));
        p.drawEllipse(QPointF(-46, 0), 4, 2);
        p.restore();
    }
    p.setOpacity(base * words);
    QFont title("Cormorant Garamond");
    title.setPixelSize(44);
    title.setLetterSpacing(QFont::AbsoluteSpacing, 4 + 10 * words);
    p.setFont(title);
    p.setPen(QColor("#e2cf9f"));
    p.drawText(QRectF(0, c.y() + r + 20, width(), 56), Qt::AlignCenter, m_mode == Opening ? QStringLiteral("AUREA") : QStringLiteral("À bientôt"));
    QFont small("Inter");
    small.setPixelSize(13);
    p.setFont(small);
    p.setPen(QColor("#8f8c86"));
    p.drawText(QRectF(0, c.y() + r + 78, width(), 24), Qt::AlignCenter,
               m_mode == Opening ? QStringLiteral("Gestion des matières premières") : QStringLiteral("Votre stock est enregistré."));
    p.setOpacity(base);
}

void StitchAnimation::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double ms = m_clock.elapsed();
    if (m_mode == Opening) {
        const double open = Motion::part(ms, 1650, OpenTime);
        curtains(p, open);
        p.setOpacity(1 - Motion::both(open * 2.4));
        emblem(p, Motion::both(Motion::part(ms, 100, 1250)), Motion::soft(Motion::part(ms, 700, 1300)));
    } else {
        curtains(p, 1 - Motion::part(ms, 0, 650));
        p.setOpacity(Motion::part(ms, 450, 750));
        emblem(p, Motion::both(Motion::part(ms, 500, 1250)), Motion::soft(Motion::part(ms, 700, 1150)));
    }
}
