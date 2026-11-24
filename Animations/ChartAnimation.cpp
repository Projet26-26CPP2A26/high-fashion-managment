#include "ChartAnimation.h"
#include "AnimationSpeed.h"
#include "pdfexport.h"
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

ChartAnimation::ChartAnimation(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setGeometry(parent->rect());
    parent->installEventFilter(this);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (m_clock.elapsed() > 2600)
            m_timer.stop();
        update();
    });
}

void ChartAnimation::setData(const QVector<QPair<QString, double>> &data)
{
    if (data == m_data)
        return;
    m_data = data;
    replay();
}

void ChartAnimation::replay()
{
    m_clock.start();
    m_timer.start(Motion::frame(this));
}

bool ChartAnimation::eventFilter(QObject *object, QEvent *event)
{
    if (object == parent() && event->type() == QEvent::Resize)
        setGeometry(parentWidget()->rect());
    if (object == parent() && event->type() == QEvent::Show)
        replay();
    return QWidget::eventFilter(object, event);
}

void ChartAnimation::mouseMoveEvent(QMouseEvent *event)
{
    const int row = m_data.isEmpty() ? -1 : int(event->position().y() / (height() / double(m_data.size())));
    if (row != m_hover) {
        m_hover = row;
        update();
    }
}

void ChartAnimation::leaveEvent(QEvent *)
{
    m_hover = -1;
    update();
}

void ChartAnimation::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#232326"));
    QFont font("Segoe UI");
    font.setPixelSize(13);
    p.setFont(font);
    if (m_data.isEmpty()) {
        p.setPen(QColor("#8f8c86"));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("Ajoutez des matières pour afficher les statistiques."));
        return;
    }
    double max = 1;
    for (const auto &d : m_data)
        max = std::max(max, d.second);
    const double ms = m_clock.isValid() ? m_clock.elapsed() : 1e9;
    const double left = 124, right = 140, step = height() / double(m_data.size()), bar = std::min(26.0, step - 10), full = width() - left - right;
    for (int i = 0; i < m_data.size(); ++i) {
        const double grow = Motion::soft(Motion::part(ms, 90 * i, 90 * i + 900)), y = i * step + (step - bar) / 2;
        const bool hover = i == m_hover;
        p.setOpacity(Motion::part(ms, 90 * i, 90 * i + 300));
        p.setPen(QColor(hover ? "#f1e1b4" : "#b9b4a8"));
        p.drawText(QRectF(0, y, left - 14, bar), Qt::AlignVCenter | Qt::AlignRight, m_data[i].first);
        const QRectF track(left, y, full, bar);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#2e2e32"));
        p.drawRoundedRect(track, 6, 6);
        const QRectF value(left, y, std::max(6.0, full * m_data[i].second / max * grow), bar);
        QLinearGradient gold(left, 0, left + full, 0);
        gold.setColorAt(0, QColor(hover ? "#d8bd86" : "#a98d5b"));
        gold.setColorAt(1, QColor("#f1e1b4"));
        if (hover) {
            p.setPen(QPen(QColor(241, 225, 180, 60), 6));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(value, 6, 6);
            p.setPen(Qt::NoPen);
        }
        p.setBrush(gold);
        p.drawRoundedRect(value, 6, 6);
        QPainterPath clip;
        clip.addRoundedRect(value, 6, 6);
        const double shine = Motion::part(ms, 900 + 110 * i, 1700 + 110 * i);
        if (shine > 0 && shine < 1) {
            p.save();
            p.setClipPath(clip);
            const double x = value.left() - 60 + (value.width() + 120) * shine;
            QLinearGradient light(x - 40, 0, x + 40, 0);
            light.setColorAt(0, QColor(255, 255, 255, 0));
            light.setColorAt(0.5, QColor(255, 255, 255, 120));
            light.setColorAt(1, QColor(255, 255, 255, 0));
            p.fillRect(value, light);
            p.restore();
        }
        if (grow < 1) {                       // tête lumineuse
            QRadialGradient head(value.right(), value.center().y(), 18);
            head.setColorAt(0, QColor(255, 246, 220, 200));
            head.setColorAt(1, QColor(255, 246, 220, 0));
            p.fillRect(QRectF(value.right() - 18, value.center().y() - 18, 36, 36), head);
        }
        p.setPen(QColor("#f3f0ea"));
        p.drawText(QRectF(left + full + 14, y, right - 14, bar), Qt::AlignVCenter, StockReports::money(m_data[i].second * grow));
    }
}
