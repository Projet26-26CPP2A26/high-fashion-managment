#include "OrderAnimation.h"
#include "AnimationSpeed.h"
#include <QPainter>
#include <QPainterPath>

OrderAnimation::OrderAnimation(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(64);
    m_life.start();
    connect(&m_timer, &QTimer::timeout, this, [this] {
        m_position += (m_step - m_position) * 0.12;
        if (isVisible())
            update();
    });
    m_timer.start(Motion::frame(this));
}

void OrderAnimation::setStep(int step)
{
    m_step = step;
}

void OrderAnimation::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QString titles[3] = {QStringLiteral("Choisir la matière"), QStringLiteral("Envoyer le bon de commande"),
                               QStringLiteral("Enregistrer la réception")};
    const QString texts[3] = {QStringLiteral("une ligne du tableau"), QStringLiteral("PDF pour le fournisseur"),
                              QStringLiteral("le stock remonte")};
    const double w = width() / 3.0, t = m_life.elapsed() / 1000.0;
    for (int i = 0; i < 3; ++i) {
        const double lit = std::clamp(m_position - i, 0.0, 1.0);
        const bool current = m_step == i + 1, done = m_step > i + 1;
        const QPointF c(i * w + 24, 32);
        if (i < 2) {
            const double a = c.x() + w - 62, b = c.x() + w - 26;
            p.setPen(QPen(QColor(done ? "#c9b27c" : "#4a4a4f"), 1.6, Qt::DashLine, Qt::RoundCap));
            p.drawLine(QPointF(a, 32), QPointF(b, 32));
        }
        if (current) {
            const double wave = std::fmod(t, 1.6) / 1.6;
            QColor ring(226, 207, 159);
            ring.setAlphaF(0.6 * (1 - wave));
            p.setPen(QPen(ring, 1.6));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, 18 + 10 * Motion::soft(wave), 18 + 10 * Motion::soft(wave));
        }
        QLinearGradient gold(c - QPointF(18, 0), c + QPointF(18, 0));
        gold.setColorAt(0, QColor("#bfa370"));
        gold.setColorAt(1, QColor("#f1e1b4"));
        p.setPen(QPen(QColor(current || done ? "#e2cf9f" : "#4a4a4f"), 1.4));
        p.setBrush(QColor("#2a2a2d"));
        p.drawEllipse(c, 18, 18);
        p.setOpacity(lit);
        p.setBrush(done ? QBrush(QColor("#7fc08a")) : QBrush(gold));
        p.setPen(Qt::NoPen);
        p.drawEllipse(c, 18 * lit, 18 * lit);
        p.setOpacity(1);
        p.setPen(QColor(lit > 0.5 ? "#1c1c1e" : "#b9b4a8"));
        QFont bold = font();
        bold.setBold(true);
        p.setFont(bold);
        if (done) {
            QPainterPath mark(c + QPointF(-6, 0));
            mark.lineTo(c + QPointF(-1.5, 5));
            mark.lineTo(c + QPointF(7, -5));
            p.setPen(QPen(QColor("#15201a"), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawPath(mark);
        } else {
            p.drawText(QRectF(c.x() - 18, c.y() - 18, 36, 36), Qt::AlignCenter, QString::number(i + 1));
        }
        const QRectF area(c.x() + 30, 10, w - 100, 24);
        p.setPen(QColor(current ? "#f1e1b4" : done ? "#b7ccb3" : "#b9b4a8"));
        p.drawText(area, Qt::AlignVCenter, p.fontMetrics().elidedText(titles[i], Qt::ElideRight, int(area.width())));
        p.setFont(font());
        p.setPen(QColor("#8f8c86"));
        p.drawText(area.translated(0, 22), Qt::AlignVCenter,
                   p.fontMetrics().elidedText(texts[i], Qt::ElideRight, int(area.width())));
    }
}
