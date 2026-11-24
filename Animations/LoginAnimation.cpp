#include "LoginAnimation.h"
#include "UiAnimations.h"
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
}

LoginScene::LoginScene(QWidget *parent)
    : QWidget(parent)
{
    const qreal ratio = devicePixelRatioF();
    m_logo = QPixmap(":/assets/logo_aurea.png")
                 .scaled(QSize(124, 124) * ratio, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_logo.setDevicePixelRatio(ratio);
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        if (!isVisible()) {
            return;
        }
        if (!m_clock.isValid()) {
            m_clock.start();
        }
        m_t = m_offset + m_clock.elapsed() / 1000.0;
        update();
    });
    timer->setTimerType(Qt::PreciseTimer);
    timer->start(Fx::ambientInterval(this));
}

void LoginScene::setLogoVisible(bool visible)
{
    m_logoVisible = visible;
    if (visible) {
        repaint();
    } else {
        update();
    }
}

QRect LoginScene::logoRect() const
{
    const QSize size = (QSizeF(m_logo.size()) / m_logo.devicePixelRatio()).toSize();
    return QRect(
        mapToGlobal(QPoint(qRound(width() / 2.0 - size.width() / 2.0), qRound(height() * 0.33 - size.height() / 2.0))),
        size);
}

double LoginScene::wave(double x, int line) const
{
    const double u = x / width();
    return height() * (0.865 + line * 0.028)
        + std::sin(u * 5.2 + m_t * (0.55 + line * 0.06) + line * 0.9) * (13 + line * 2)
        + std::sin(u * 11 - m_t * 0.35 + line) * 7;
}

void LoginScene::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal ratio = devicePixelRatioF();
    const QSize pixels = (QSizeF(size()) * ratio).toSize();
    if (m_back.size() != pixels) {
        m_back = QPixmap(pixels);
        m_back.setDevicePixelRatio(ratio);
        QPainter b(&m_back);
        QLinearGradient back(0, 0, width(), height());
        back.setColorAt(0, QColor("#131315"));
        back.setColorAt(0.6, QColor("#211e17"));
        back.setColorAt(1, QColor("#453a25"));
        b.fillRect(rect(), back);
    }
    p.drawPixmap(0, 0, m_back);

    const QPointF center(width() / 2.0, height() * 0.33);
    const double breath = 0.5 + 0.5 * std::sin(m_t * 0.9);
    QRadialGradient glow(center, 190 + 14 * breath);
    glow.setColorAt(0, QColor(226, 207, 159, int((34 + 16 * breath) * ease(m_t / 1.6))));
    glow.setColorAt(1, QColor(226, 207, 159, 0));
    const double reach = 191 + 14 * breath;
    p.fillRect(QRectF(center.x() - reach, center.y() - reach, reach * 2, reach * 2).toAlignedRect() & rect(), glow);

    for (int line = 4; line >= 0; --line) {
        const double drawn = width() * ease((m_t - 0.2 - line * 0.12) / 2.0);
        if (drawn <= 4) {
            continue;
        }
        QPainterPath path(QPointF(0, wave(0, line)));
        for (double x = 4; x <= drawn; x += 4) {
            path.lineTo(x, wave(x, line));
        }
        p.setPen(QPen(QColor(226, 207, 159, line == 0 ? 190 : 78 - line * 13), line == 0 ? 1.8 : 1.1));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

    const double cycle = std::fmod(std::max(0.0, m_t - 2.2), 9.0) / 9.0;
    if (m_t > 2.2) {
        const double head = cycle * (width() + 120) - 40;
        for (double x = 10; x < std::min(head - 34, double(width())); x += 22) {
            const double a = std::clamp(1 - (head - x) / (width() * 1.1), 0.0, 1.0);
            p.setPen(QPen(QColor(255, 244, 210, int(210 * a)), 2.6, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(x, wave(x, 0) - 14), QPointF(x + 11, wave(x + 11, 0) - 14));
        }
        const QPointF tip(head, wave(head, 0) - 14), tail(head - 36, wave(head - 36, 0) - 14 - 5);
        p.setPen(QPen(QColor("#fff7e0"), 2.4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(tail, tip);
        p.setPen(QPen(QColor("#c9b27c"), 1.3));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(tail, 3.2, 2.2);
    }

    const double ring = m_ringsVisible ? ease((m_t - 0.15) / 1.3) : 0;
    p.setPen(QPen(QColor(226, 207, 159, 150), 1.4, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(center.x() - 82, center.y() - 82, 164, 164), int((90 + m_t * 12) * 16), -int(300 * 16 * ring));
    p.setPen(QPen(QColor(226, 207, 159, 70), 1));
    p.drawArc(QRectF(center.x() - 92, center.y() - 92, 184, 184), int((200 - m_t * 9) * 16), int(220 * 16 * ring));
    const double shown = ease((m_t - 0.35) / 0.9);
    p.save();
    p.setOpacity(shown);
    p.translate(center);
    p.scale(0.86 + 0.14 * shown, 0.86 + 0.14 * shown);
    const QSizeF size = QSizeF(m_logo.size()) / m_logo.devicePixelRatio();
    if (m_logoVisible) {
        p.drawPixmap(QPointF(qRound(center.x() - size.width() / 2) - center.x(),
                         qRound(center.y() - size.height() / 2) - center.y()),
            m_logo);
    }
    p.restore();

    QFont brand("Cormorant Garamond", 42);
    brand.setWeight(QFont::DemiBold);
    p.setFont(brand);
    const QString word = QStringLiteral("AUREA");
    const QFontMetricsF fm(brand);
    const double gap = 5;
    double total = -gap;
    for (const QChar c : word) {
        total += fm.horizontalAdvance(c) + gap;
    }
    double x = (width() - total) / 2;
    const double base = height() * 0.33 + 152;
    const double sweep = std::fmod(m_t, 5.0) / 5.0 * 3.0 - 0.6;
    for (int i = 0; i < word.size(); ++i) {
        const double a = ease((m_t - 0.7 - i * 0.11) / 0.7);
        const double shine = std::exp(-std::pow((sweep - i / 4.0) / 0.22, 2));
        QColor color(233 + int(22 * shine), 216 + int(34 * shine), 168 + int(70 * shine));
        color.setAlphaF(a);
        p.setPen(color);
        p.drawText(QPointF(x, base + 14 * (1 - a)), QString(word[i]));
        x += fm.horizontalAdvance(word[i]) + gap;
    }

    const double text = ease((m_t - 1.4) / 0.9);
    QFont small("Inter", 10);
    p.setFont(small);
    p.setPen(QColor(201, 196, 184, int(255 * text)));
    p.drawText(QRectF(0, base + 22, width(), 24), Qt::AlignCenter, QStringLiteral("Atelier haute couture"));
    p.setPen(QColor(160, 154, 142, int(255 * text)));
    p.drawText(QRectF(0, base + 50, width(), 44), Qt::AlignCenter,
        QStringLiteral("Gestion des employés,\nprésence et temps de travail en direct"));
}
