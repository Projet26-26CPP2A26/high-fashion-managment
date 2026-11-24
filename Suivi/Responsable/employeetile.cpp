#include "employeetile.h"
#include "UiAnimations.h"
#include "icons.h"
#include "AnimationSpeed.h"
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <algorithm>
#include <cmath>

QColor statusColor(const QString &s)
{
    if (s == QStringLiteral("Actif")) {
        return QColor("#7fc08a");
    }
    if (s == QStringLiteral("En pause")) {
        return QColor("#e6c27a");
    }
    if (s == QStringLiteral("Inactif")) {
        return QColor("#e3a06d");
    }
    if (s == QStringLiteral("PC hors ligne")) {
        return QColor("#e08f8f");
    }
    return QColor("#8f8c86");
}

QStringList sessionAlerts(const WorkSession &s)
{
    QStringList out;
    if (s.status() == QStringLiteral("PC hors ligne")) {
        out << QStringLiteral("PC hors ligne");
    }
    if (s.status() == QStringLiteral("Inactif")) {
        out << QStringLiteral("Inactif");
    }
    if (s.pause > WorkRules::Pause) {
        out << QStringLiteral("Pause dépassée");
    }
    if (!s.ended() && s.taskPlanned > 0 && s.taskSpent > s.taskPlanned) {
        out << QStringLiteral("Tâche en retard");
    }
    return out;
}

EmployeeTile::EmployeeTile(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(212);
    setMinimumWidth(240);
    setCursor(Qt::PointingHandCursor);
    m_hoverAnimation.setDuration(Motion::Quick);
    m_hoverAnimation.setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(&m_hoverAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_hover = v.toReal();
        update();
    });
    m_barAnimation.setDuration(Motion::Slow);
    m_barAnimation.setStartValue(0.0);
    m_barAnimation.setEndValue(1.0);
    m_barAnimation.setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(&m_barAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_progress = m_fromProgress + (m_toProgress - m_fromProgress) * v.toReal();
        m_day = m_fromDay + (m_toDay - m_fromDay) * v.toReal();
        update();
    });
    m_flashAnimation.setDuration(Motion::Glow);
    m_flashAnimation.setStartValue(1.0);
    m_flashAnimation.setEndValue(0.0);
    QObject::connect(&m_flashAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_flash = v.toReal();
        update();
    });
}

void EmployeeTile::enter(int delay)
{
    m_enter = 0;
    auto *a = new QVariantAnimation(this);
    a->setDuration(Motion::Slow);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setEasingCurve(QEasingCurve::OutCubic);
    connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_enter = v.toReal();
        update();
    });
    QTimer::singleShot(delay, a, [a] {
        a->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void EmployeeTile::animateHover(qreal to)
{
    m_hoverAnimation.stop();
    m_hoverAnimation.setStartValue(m_hover);
    m_hoverAnimation.setEndValue(to);
    m_hoverAnimation.start();
}

void EmployeeTile::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->pos()) && clicked) {
        clicked();
    }
}

void EmployeeTile::setSession(const WorkSession &s, bool selected)
{
    const QString status = s.status();
    if (!m_first && (status != m_status || s.task != m_session.task)) {
        m_flashAnimation.start();
    }
    m_first = false;
    m_session = s;
    m_status = status;
    m_selected = selected;
    m_alerts = sessionAlerts(s);
    const qreal progress = s.taskPlanned > 0 ? std::min(1.0, s.taskSpent / qreal(s.taskPlanned)) : 0;
    const qreal day = std::min(1.0, s.work / qreal(WorkRules::Day));
    if (std::abs(progress - m_toProgress) > 0.004 || std::abs(day - m_toDay) > 0.004) {
        m_fromProgress = m_progress;
        m_fromDay = m_day;
        m_toProgress = progress;
        m_toDay = day;
        m_barAnimation.stop();
        m_barAnimation.start();
    }
    setToolTip(m_alerts.isEmpty() ? QString() : m_alerts.join(", "));
    update();
}

void EmployeeTile::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor color = statusColor(m_status);
    p.setOpacity(m_enter);
    p.translate(0, 10 * (1 - m_enter));
    const qreal lift = 1 * m_hover;
    const QRectF card = QRectF(rect()).adjusted(3, 4 - lift, -3, -4 - lift);

    QLinearGradient back(card.topLeft(), card.bottomRight());
    back.setColorAt(0, QColor(m_hover > 0.5 ? "#2f2e2c" : "#29292c"));
    back.setColorAt(1, QColor("#202023"));
    QColor border = m_selected ? QColor("#e2cf9f") : QColor("#38383d");
    if (!m_selected && m_hover > 0) {
        border = QColor("#8f7f58");
    }
    p.setBrush(back);
    p.setPen(QPen(border, m_selected ? 2 : 1));
    p.drawRoundedRect(card, 14, 14);
    if (m_flash > 0) {
        QColor glow = color;
        glow.setAlphaF(0.10 * m_flash);
        p.setBrush(glow);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(card, 14, 14);
    }
    QPainterPath clip;
    clip.addRoundedRect(card, 14, 14);
    p.save();
    p.setClipPath(clip);
    p.fillRect(QRectF(card.left(), card.top(), 4, card.height()), color);
    p.restore();

    const int x = int(card.left()) + 18, right = int(card.right()) - 16, y = int(card.top());
    const int width = right - x;
    QFont base = font();
    const auto text = [&](const QRect &r, const QString &t, const QColor &c, qreal size, int weight, int align) {
        QFont f = base;
        f.setPointSizeF(size);
        f.setWeight(QFont::Weight(weight));
        p.setFont(f);
        p.setPen(c);
        p.drawText(r, align | Qt::AlignVCenter, p.fontMetrics().elidedText(t, Qt::ElideRight, r.width()));
    };

    const QPointF center(x + 21, y + 38);
    m_avatar = QRect(x - 6, y + 11, 54, 54);
    if (m_status == QStringLiteral("Actif")) {
        QColor halo = color;
        halo.setAlphaF(0.22 * (1 - m_pulse));
        p.setPen(Qt::NoPen);
        p.setBrush(halo);
        p.drawEllipse(center, 21 + 4 * m_pulse, 21 + 4 * m_pulse);
    }
    p.setPen(QPen(color, 2));
    p.setBrush(QColor("#3a3427"));
    p.drawEllipse(center, 21, 21);
    QString initials;
    for (const QString &part : m_session.name.split(' ', Qt::SkipEmptyParts)) {
        if (initials.size() < 2) {
            initials += part[0].toUpper();
        }
    }
    text(QRect(int(center.x()) - 20, int(center.y()) - 14, 40, 28), initials, QColor("#e9d8a8"), 10.5, QFont::Bold,
        Qt::AlignHCenter);

    const int nameX = x + 54;
    if (m_avatar.contains(event->rect())) {
        return;
    }
    text(QRect(nameX, y + 16, right - nameX - 20, 22), m_session.name, QColor("#ffffff"), 11, QFont::DemiBold,
        Qt::AlignLeft);
    text(QRect(nameX, y + 38, right - nameX, 18), m_session.position, QColor("#8f8c86"), 9, QFont::Normal,
        Qt::AlignLeft);

    if (m_session.login.isValid() && !m_session.ended() && !m_session.via.isEmpty()) {
        p.drawPixmap(QPointF(right - 16, y + 18),
            Icons::pixmap(m_session.via == "BADGE" ? Icons::Office : Icons::Online,
                QColor(m_session.via == "BADGE" ? "#e2cf9f" : "#8fb3c9"), 16));
    }

    QColor soft = color;
    soft.setAlpha(38);
    p.setPen(Qt::NoPen);
    p.setBrush(soft);
    p.drawRoundedRect(QRectF(x, y + 70, 116, 24), 12, 12);
    p.setBrush(color);
    p.drawEllipse(QPointF(x + 13, y + 82), 3.2, 3.2);
    text(QRect(x + 23, y + 70, 92, 24), m_status, color, 9, QFont::DemiBold, Qt::AlignLeft);
    text(QRect(right - 110, y + 68, 110, 28), clockText(m_session.work), QColor("#f4e7bc"), 14, QFont::Bold,
        Qt::AlignRight);

    const bool late = m_session.taskPlanned > 0 && m_session.taskSpent > m_session.taskPlanned;
    text(QRect(x, y + 102, width - 72, 22),
        m_session.task.isEmpty() ? QStringLiteral("Aucune tâche en cours") : m_session.task, QColor("#eeeae1"), 9.5,
        QFont::Medium, Qt::AlignLeft);
    if (m_session.taskPlanned > 0) {
        text(QRect(right - 70, y + 102, 70, 22),
            QString::number(std::min(999, int(m_session.taskSpent * 100LL / m_session.taskPlanned))) + " %",
            QColor(late ? "#e08f8f" : "#b9b4a8"), 9, QFont::Medium, Qt::AlignRight);
    }
    const auto bar = [&](int top, qreal value, const QColor &a, const QColor &b) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#3b3b40"));
        p.drawRoundedRect(QRectF(x, top, width, 5), 2.5, 2.5);
        if (value <= 0) {
            return;
        }
        QLinearGradient g(x, 0, right, 0);
        g.setColorAt(0, a);
        g.setColorAt(1, b);
        p.setBrush(g);
        p.drawRoundedRect(QRectF(x, top, std::max(5.0, width * value), 5), 2.5, 2.5);
    };
    bar(y + 127, m_progress, late ? QColor("#c27575") : QColor("#bfa370"),
        late ? QColor("#e8a3a3") : QColor("#e2cf9f"));

    const int third = width / 3;
    const auto mini = [&](int i, const QString &label, const QString &value, const QColor &c) {
        text(QRect(x + i * third, y + 140, third, 16), label, QColor("#8f8c86"), 7.5, QFont::Normal, Qt::AlignLeft);
        text(QRect(x + i * third, y + 155, third, 18), value, c, 9, QFont::DemiBold, Qt::AlignLeft);
    };
    const QString hm = QStringLiteral("%1 min");
    mini(0, QStringLiteral("Pause"), hm.arg(m_session.pause / 60),
        QColor(m_session.pause > WorkRules::Pause ? "#e08f8f" : "#d9d5cb"));
    mini(1, QStringLiteral("Inactif"), hm.arg(m_session.idle / 60), QColor("#d9d5cb"));
    mini(2, QStringLiteral("Arrivée"),
        m_session.login.isValid() ? m_session.login.toString("HH:mm") : QStringLiteral("—"), QColor("#d9d5cb"));
    if (m_alerts.isEmpty()) {
        bar(y + 184, m_day, QColor("#4f7d58"), QColor("#7fc08a"));
    } else {
        text(QRect(x, y + 177, width, 18), QStringLiteral("▲  ") + m_alerts.join("  ·  "), QColor("#e8a3a3"), 8.5,
            QFont::DemiBold, Qt::AlignLeft);
    }
}

StarRating::StarRating(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(230, 50);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    m_clock.start();
    m_frame.start();
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(Fx::frameInterval(this));
    connect(m_timer, &QTimer::timeout, this, [this] {
        step();
    });
}

int StarRating::starAt(double x) const
{
    return std::clamp(int(x) / 45 + 1, 1, 5);
}

void StarRating::setValue(int v)
{
    m_value = v;
    m_clicked = -1;
    m_ring = 1;
    for (int i = 0; i < 5; ++i) {
        m_fill[i] = 0;
        m_scale[i] = 0.7;
    }
    m_clock.restart();
    m_frame.restart();
    m_timer->start();
    update();
}

void StarRating::step()
{
    const double dt = std::min(m_frame.nsecsElapsed() / 1e9, 0.034);
    m_frame.restart();
    const double k = 1 - std::exp(-dt * 13), elapsed = m_clock.elapsed() / 1000.0;
    const int shown = m_hover ? m_hover : m_value;
    bool moving = false;
    for (int i = 0; i < 5; ++i) {
        if (elapsed < 0.15 + i * 0.07) {
            moving = true;
            continue;
        }
        const double fill = i < shown ? 1 : 0, scale = m_hover == i + 1 ? 1.16 : 1.0;
        m_fill[i] += (fill - m_fill[i]) * k;
        m_scale[i] += (scale - m_scale[i]) * k;
        moving |= std::abs(fill - m_fill[i]) > 0.004 || std::abs(scale - m_scale[i]) > 0.004;
    }
    if (m_ring < 1) {
        m_ring = std::min(1.0, m_ring + dt / 0.45);
        moving = true;
    }
    if (!moving) {
        m_timer->stop();
    }
    update();
}

void StarRating::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    for (int i = 0; i < 5; ++i) {
        const QPointF c(25 + i * 45, height() / 2.0);
        if (m_clicked == i && m_ring < 1) {
            const double out = 1 - std::pow(1 - m_ring, 3);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(244, 210, 122, int(150 * (1 - m_ring))), 1.6));
            p.drawEllipse(c, 13 + 10 * out, 13 + 10 * out);
        }
        QPainterPath star;
        for (int n = 0; n < 10; ++n) {
            const double r = (n % 2 ? 6.6 : 15.5) * m_scale[i], a = -M_PI / 2 + n * M_PI / 5;
            const QPointF point(c.x() + r * std::cos(a), c.y() + r * std::sin(a));
            n ? star.lineTo(point) : star.moveTo(point);
        }
        star.closeSubpath();
        p.setPen(QPen(QColor("#5a5548"), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(QColor("#1f1f22"));
        p.drawPath(star);
        if (m_fill[i] < 0.01) {
            continue;
        }
        p.setOpacity(m_fill[i]);
        QLinearGradient gold(c - QPointF(12, 14), c + QPointF(12, 14));
        gold.setColorAt(0, QColor("#fbe7ae"));
        gold.setColorAt(1, QColor("#dfb655"));
        p.setPen(QPen(QColor("#f4d27a"), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(gold);
        p.drawPath(star);
        p.setOpacity(1);
    }
}

void StarRating::mouseMoveEvent(QMouseEvent *e)
{
    const int star = starAt(e->position().x());
    if (star == m_hover) {
        return;
    }
    m_hover = star;
    m_frame.restart();
    m_timer->start();
}

void StarRating::mousePressEvent(QMouseEvent *e)
{
    m_value = starAt(e->position().x());
    m_clicked = m_value - 1;
    m_ring = 0;
    m_frame.restart();
    m_timer->start();
}

void StarRating::leaveEvent(QEvent *)
{
    m_hover = 0;
    m_frame.restart();
    m_timer->start();
}

DayTimeline::DayTimeline(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(64);
}

void DayTimeline::add(const QString &state, int seconds)
{
    if (!m_parts.isEmpty() && m_parts.last().first == state) {
        m_parts.last().second += seconds;
    } else {
        m_parts.append(qMakePair(state, seconds));
    }
    update();
}

void DayTimeline::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF track(0, 6, width(), 22);
    QPainterPath clip;
    clip.addRoundedRect(track, 8, 8);
    p.save();
    p.setClipPath(clip);
    p.fillRect(track, QColor("#3b3b40"));
    qint64 total = 0;
    for (const auto &part : m_parts) {
        total += part.second;
    }
    qreal x = 0;
    for (const auto &part : m_parts) {
        const qreal w = total ? track.width() * part.second / total : 0;
        p.fillRect(QRectF(x, track.top(), w + 0.5, track.height()), statusColor(part.first));
        x += w;
    }
    p.restore();
    int lx = 2;
    QFont f = font();
    f.setPointSizeF(8.5);
    p.setFont(f);
    for (const QString &s : {QStringLiteral("Actif"), QStringLiteral("En pause"), QStringLiteral("Inactif"),
             QStringLiteral("PC hors ligne")}) {
        p.setPen(Qt::NoPen);
        p.setBrush(statusColor(s));
        p.drawEllipse(QPointF(lx + 4, 46), 4, 4);
        p.setPen(QColor("#b9b4a8"));
        p.drawText(QPointF(lx + 14, 50), s);
        lx += p.fontMetrics().horizontalAdvance(s) + 38;
    }
}

ProgressRing::ProgressRing(int size, QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(size * 7 / 10, size * 7 / 10);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_animation.setDuration(Motion::Slow);
    m_animation.setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_value = v.toReal();
        update();
    });
}

void ProgressRing::setValue(qreal ratio, const QString &text, const QString &caption)
{
    m_text = text;
    m_caption = caption;
    ratio = std::clamp(ratio, 0.0, 1.0);
    if (std::abs(ratio - m_target) > 0.002) {
        m_target = ratio;
        m_animation.stop();
        m_animation.setStartValue(m_value);
        m_animation.setEndValue(ratio);
        m_animation.start();
    }
    update();
}

void ProgressRing::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double side = std::min({double(width()), double(height()), 320.0}), k = side / 170.0;
    const QRectF r = QRectF((width() - side) / 2, (height() - side) / 2, side, side).adjusted(12, 12, -12, -12);
    p.setPen(QPen(QColor("#38383d"), 10));
    p.drawEllipse(r);
    if (m_value > 0) {
        QConicalGradient ringBrush(r.center(), 90);
        ringBrush.setColorAt(0, QColor("#e2cf9f"));
        ringBrush.setColorAt(0.5, QColor("#bfa370"));
        ringBrush.setColorAt(1, QColor("#e2cf9f"));
        p.setPen(QPen(QColor(226, 207, 159, 40), 18, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(r, 90 * 16, -int(m_value * 360 * 16));
        p.setPen(QPen(ringBrush, 10, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(r, 90 * 16, -int(m_value * 360 * 16));
    }
    QFont f = font();
    f.setPointSizeF(std::clamp(18 * k, 13.0, 30.0));
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor("#ffffff"));
    p.drawText(rect().translated(0, side < 118 ? 0 : int(-10 * k)), Qt::AlignCenter, m_text);
    if (side < 118) {
        return;
    }
    f.setPointSizeF(std::clamp(8.5 * k, 8.0, 12.0));
    f.setBold(false);
    p.setFont(f);
    p.setPen(QColor("#8f8c86"));
    p.drawText(rect().translated(0, int(22 * k)), Qt::AlignCenter, m_caption);
}
