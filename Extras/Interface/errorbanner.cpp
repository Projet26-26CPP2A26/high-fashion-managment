#include "errorbanner.h"
#include "UiAnimations.h"
#include <QPainter>
#include <QTimer>
#include <algorithm>
#include <cmath>

ErrorBanner::ErrorBanner(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(Closed);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ErrorBanner::tick);
}

void ErrorBanner::showError(const QString &title, const QString &text)
{
    if (locked()) {
        return;
    }
    m_title = title;
    m_text = text;
    open();
}

void ErrorBanner::lock(int seconds)
{
    m_locked = seconds;
    m_clock.start();
    m_title = QStringLiteral("Trop d'essais");
    m_text = QStringLiteral("Vous pouvez réessayer.");
    open();
}

void ErrorBanner::clear()
{
    if (!m_shown || locked()) {
        return;
    }
    m_shown = false;
    m_open.go(0, Motion::Base);
    m_timer->start(Fx::ambientInterval(this));
}

void ErrorBanner::open()
{
    if (!m_shown) {
        m_open.go(1, Motion::Slow);
        m_icon.jump(0);
        m_icon.go(1, Motion::Slow);
    }
    m_shown = true;
    m_words.jump(0);
    m_words.go(1, Motion::Slow);
    m_pulse.jump(1);
    m_pulse.go(0, Motion::Glow);
    setAccessibleName(m_title);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->start(Fx::ambientInterval(this));
    update();
}

double ErrorBanner::remaining() const
{
    return std::max(0.0, m_locked - m_clock.elapsed() / 1000.0);
}

void ErrorBanner::tick()
{
    if (locked() && remaining() <= 0) {
        m_locked = 0;
        clear();
        if (unlocked) {
            unlocked();
        }
    }
    const int height = Closed + int(std::lround((Opened - Closed) * m_open.value()));
    if (height != this->height()) {
        setFixedHeight(height);
    }
    if (!locked() && !m_open.running() && !m_pulse.running() && !m_icon.running() && !m_words.running()) {
        m_timer->stop();
    }
    update();
}

void ErrorBanner::paintEvent(QPaintEvent *)
{
    const double shown = m_open.value();
    if (shown < 0.01) {
        return;
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(std::pow(shown, 1.6));
    const double pulse = m_pulse.value();
    const QRectF box = QRectF(0.5, 1, width() - 1, height() - 2);

    QLinearGradient back(box.topLeft(), box.topRight());
    back.setColorAt(0, QColor(224, 143, 143, int(34 + 26 * pulse)));
    back.setColorAt(1, QColor(224, 143, 143, int(14 + 14 * pulse)));
    p.setPen(QPen(QColor(224, 143, 143, int(90 + 110 * pulse)), 1));
    p.setBrush(back);
    p.drawRoundedRect(box, 10, 10);

    const QPointF center(box.left() + 30, box.center().y());
    if (locked()) {
        paintCountdown(p, center);
    } else {
        paintWarning(p, center, m_icon.value());
    }

    const double words = m_words.value();
    const double left = box.left() + 58 + 6 * (1 - words), space = box.right() - left - 12;
    p.setOpacity(std::pow(shown, 4) * words);
    QFont f = font();
    f.setPointSizeF(10);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(QColor("#f6c9ca"));
    p.drawText(QRectF(left, box.center().y() - 19, space, 20), Qt::AlignVCenter,
        p.fontMetrics().elidedText(m_title, Qt::ElideRight, int(space)));
    f.setPointSizeF(9);
    f.setWeight(QFont::Normal);
    p.setFont(f);
    p.setPen(QColor("#d49a9b"));
    QString text = m_text;
    if (locked()) {
        const int seconds = int(std::ceil(remaining()));
        text = QStringLiteral("Réessayez dans %1 seconde%2").arg(seconds).arg(seconds > 1 ? "s" : "");
    }
    p.drawText(QRectF(left, box.center().y(), space, 18), Qt::AlignVCenter,
        p.fontMetrics().elidedText(text, Qt::ElideRight, int(space)));
}

void ErrorBanner::paintWarning(QPainter &p, const QPointF &center, double grow) const
{
    const double radius = 14 * (0.7 + 0.3 * grow);
    p.setPen(QPen(QColor("#e08f8f"), 1.6));
    p.setBrush(QColor(224, 143, 143, 36));
    p.drawEllipse(center, radius, radius);
    p.setPen(QPen(QColor("#f0b9bb"), 2.4, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(center + QPointF(0, -6), center + QPointF(0, -6 + 7 * grow));
    if (grow > 0.6) {
        p.drawPoint(center + QPointF(0, 6));
    }
}

void ErrorBanner::paintCountdown(QPainter &p, const QPointF &center) const
{
    const QRectF ring(center.x() - 15, center.y() - 15, 30, 30);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(224, 143, 143, 50), 2.6));
    p.drawEllipse(ring);
    p.setPen(QPen(QColor("#e08f8f"), 2.6, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(ring, 90 * 16, int(360 * 16 * remaining() / m_locked));
    QFont f = font();
    f.setPointSizeF(9.5);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(QColor("#f6c9ca"));
    p.drawText(ring, Qt::AlignCenter, QString::number(int(std::ceil(remaining()))));
}
