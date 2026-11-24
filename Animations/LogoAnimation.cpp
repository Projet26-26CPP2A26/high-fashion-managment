#include "LogoAnimation.h"
#include <QEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace {
double ease(double v)
{
    v = std::clamp(v, 0.0, 1.0);
    return 1 - std::pow(1 - v, 3);
}
QTimer *pulse(QWidget *w, int interval)
{
    auto *timer = new QTimer(w);
    timer->setTimerType(Qt::PreciseTimer);
    QObject::connect(timer, &QTimer::timeout, w, [w] {
        if (w->isVisible()) w->update();
    });
    timer->start(interval);
    return timer;
}
}

LivingIcon::LivingIcon(Kind kind, int size, QWidget *parent) : QWidget(parent), m_kind(kind)
{
    setFixedSize(size, size);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    m_clock.start();
    pulse(this, 16);
}

void LivingIcon::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double t = m_clock.elapsed() / 1000.0, k = width() / 72.0;
    const QColor color(m_kind == Alert ? "#e6a27a" : m_kind == Check ? "#7fc08a" : "#e2cf9f");
    p.translate(width() / 2.0, height() / 2.0);
    p.scale(k, k);
    const double grow = (0.9 + 0.1 * ease(t / 0.4)), breath = 0.35;
    QRadialGradient glow(0, 0, 35);
    QColor soft = color;
    soft.setAlpha(int(26 + 30 * breath));
    glow.setColorAt(0, soft);
    soft.setAlpha(0);
    glow.setColorAt(1, soft);
    p.fillRect(QRectF(-36, -36, 72, 72), glow);
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 1; ++i) {
        QColor ring = color;
        ring.setAlpha(i ? 90 : 170);
        p.setPen(QPen(ring, i ? 1.1 : 1.7, Qt::SolidLine, Qt::RoundCap));
        const double radius = 27 + i * 5;
        p.drawArc(QRectF(-radius, -radius, radius * 2, radius * 2), int((t * (i ? -70 : 40) + i * 140) * 16), int((m_kind == Empty ? 250 : 120 + 40 * breath) * 16 * ease(t / 0.8)));
    }
    p.scale(grow, grow);
    if (m_kind != Empty) {
        soft = color;
        soft.setAlpha(30);
        p.setPen(QPen(color, 1.6));
        p.setBrush(soft);
        p.drawEllipse(QPointF(0, 0), 21, 21);
    }
    const double draw = ease((t - 0.1) / 0.4);
    p.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    if (m_kind == Check) {
        QPainterPath path(QPointF(-9, 1));
        path.lineTo(-2.5, 7.5);
        path.lineTo(10, -7);
        QPainterPath part(path.pointAtPercent(0));
        for (double s = 0.02; s <= draw; s += 0.02) part.lineTo(path.pointAtPercent(s));
        p.drawPath(part);
    } else if (m_kind == Empty) {
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 3; ++i) p.drawEllipse(QPointF(-9 + 9 * i, -3.5 * std::max(0.0, std::sin(t * 3.4 - i * 0.7))), 2.4, 2.4);
    } else {
        QFont f("Inter");
        f.setPixelSize(24);
        f.setBold(true);
        p.setFont(f);
        QColor text = color;
        text.setAlphaF(draw);
        p.setPen(text);
        p.drawText(QRectF(-20, -21, 40, 41), Qt::AlignCenter, m_kind == Question ? "?" : m_kind == Alert ? "!" : "i");
    }
}

void LivingLogo::attach(QLabel *logo)
{
    if (logo && !logo->findChild<QWidget *>("livingLogo")) new LivingLogo(logo);
}

LivingLogo::LivingLogo(QLabel *logo) : QWidget(logo), m_logo(logo)
{
    setObjectName("livingLogo");
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setGeometry(logo->rect());
    logo->installEventFilter(this);
    m_clock.start();
    pulse(this, 33);
    show();
}

bool LivingLogo::eventFilter(QObject *object, QEvent *event)
{
    if (object == parent() && event->type() == QEvent::Resize) setGeometry(static_cast<QWidget *>(object)->rect());
    return QWidget::eventFilter(object, event);
}

void LivingLogo::paintEvent(QPaintEvent *)
{
    if (m_logo->property("logoHidden").toBool()) return;
    const QPixmap image = m_logo->pixmap();
    if (image.isNull()) return;
    const QSizeF size = QSizeF(image.size()) / image.devicePixelRatio();
    const double r = std::min(size.width(), size.height()) * 0.41, t = m_clock.elapsed() / 1000.0;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(width() / 2.0, height() / 2.0);
    QPainterPath disc;
    disc.addEllipse(QPointF(0, 0), r, r);
    const double sweep = std::fmod(t, 9.0) / 9.0 * 9 - 1.6;
    p.save();
    p.setClipPath(disc);
    QLinearGradient shine(QPointF(r * (sweep - 0.2), -r), QPointF(r * (sweep + 0.2), -r * 0.82));
    shine.setColorAt(0, QColor(255, 246, 214, 0));
    shine.setColorAt(0.5, QColor(255, 246, 214, 38));
    shine.setColorAt(1, QColor(255, 246, 214, 0));
    p.fillRect(QRectF(-r, -r, r * 2, r * 2), shine);
    p.restore();
}
