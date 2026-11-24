#include "ClosingAnimation.h"
#include "AnimationSpeed.h"
#include "LinesAnimation.h"
#include <QPainter>
#include <QPainterPath>

namespace {
const double Fold = 820, End = 2100;
}

ClosingAnimation::ClosingAnimation(QWidget *window) : QWidget(window), m_shot(window->grab())
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setGeometry(window->rect());
    m_logo = QPixmap(":/assets/logo_aurea.png").scaled(QSize(150, 150) * devicePixelRatioF(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        update();
        const double ms = m_clock.elapsed();
        this->window()->setWindowOpacity(1 - Motion::both(Motion::part(ms, End - 320, End)));
        if (ms > End)
            finish();
    });
    m_timer.setTimerType(Qt::PreciseTimer);
    show();
    raise();
    m_clock.start();
    m_timer.start(Motion::frame(this));
}

void ClosingAnimation::finish()
{
    if (m_done)
        return;
    m_done = true;
    m_timer.stop();
    emit finished();
}

void ClosingAnimation::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    const double ms = m_clock.elapsed();
    const QPointF center(width() / 2.0, height() / 2.0 - 30);
    if (m_back.isNull()) {
        m_back = QPixmap(size() * devicePixelRatioF());
        m_back.setDevicePixelRatio(devicePixelRatioF());
        QPainter b(&m_back);
        QRadialGradient back(center, width() * 0.6);
        back.setColorAt(0, QColor("#26241f"));
        back.setColorAt(0.5, QColor("#1b1b1d"));
        back.setColorAt(1, QColor("#121214"));
        b.fillRect(rect(), back);
    }
    p.drawPixmap(0, 0, m_back);

    LinesAnimation::Scene lines;
    lines.area = rect();
    lines.center = center;
    lines.radius = 96;
    lines.time = ms / 1000;
    lines.drawn = Motion::both(Motion::part(ms, 150, 900));
    lines.gather = Motion::part(ms, 500, 1250);
    lines.alpha = Motion::part(ms, 150, 500);
    LinesAnimation::paint(p, lines, false);

    const double fold = Motion::both(Motion::part(ms, 0, Fold));
    if (fold < 1) {                           // la fenêtre devient une pastille
        const double w = Motion::mix(width(), 64, fold), h = Motion::mix(height(), 64, fold), r = Motion::mix(0, 32, std::min(1.0, fold * 1.6));
        const QPointF c(Motion::mix(width() / 2.0, center.x(), fold), Motion::mix(height() / 2.0, center.y(), fold));
        const QRectF card(c.x() - w / 2, c.y() - h / 2, w, h);
        QPainterPath clip;
        clip.addRoundedRect(card, r, r);
        p.save();
        p.setOpacity(1 - Motion::part(fold, 0.75, 1));
        p.setClipPath(clip);
        const double zoom = std::max(w / width(), h / height());
        p.drawPixmap(QRectF(c.x() - width() * zoom / 2, c.y() - height() * zoom / 2, width() * zoom, height() * zoom), m_shot, QRectF(m_shot.rect()));
        p.fillRect(card, QColor(20, 20, 22, int(150 * fold)));
        p.setClipping(false);
        p.setPen(QPen(QColor(226, 207, 159, int(210 * fold)), 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(card, r, r);
        p.restore();
    }
    const double in = Motion::spring(Motion::part(ms, Fold - 180, Fold + 420)), show = Motion::part(ms, Fold - 180, Fold + 120);
    const QSizeF size = QSizeF(m_logo.size()) / devicePixelRatioF() * Motion::mix(0.5, 1, in);
    p.setOpacity(show);
    p.drawPixmap(QRectF(center - QPointF(size.width() / 2, size.height() / 2), size), m_logo, QRectF(m_logo.rect()));
    p.setOpacity(1);
    LinesAnimation::paint(p, lines, true);

    const double words = Motion::soft(Motion::part(ms, Fold + 80, Fold + 560));
    QFont title("Cormorant Garamond");
    title.setPixelSize(44);
    title.setLetterSpacing(QFont::AbsoluteSpacing, 3 + 6 * words);
    p.setFont(title);
    p.setOpacity(words);
    p.setPen(QColor("#e9d8a8"));
    p.drawText(QRectF(0, center.y() + 130 + 12 * (1 - words), width(), 56), Qt::AlignCenter, QStringLiteral("À bientôt"));
    QFont small("Inter");
    small.setPixelSize(14);
    p.setFont(small);
    p.setPen(QColor("#b9b4a8"));
    p.drawText(QRectF(0, center.y() + 188, width(), 26), Qt::AlignCenter, QStringLiteral("Votre stock est enregistré."));
}
