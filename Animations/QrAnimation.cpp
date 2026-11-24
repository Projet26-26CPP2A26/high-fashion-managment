#include "QrAnimation.h"
#include "Motion.h"
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>

namespace {
const int Margin = 16;              // place des coins dorés autour du QR
const double Sew = 1500, Scan = 900, Result_ = 1600;
}

QrAnimation::QrAnimation(QWidget *parent) : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("Cliquer pour rejouer l'animation"));
    if (parent) {
        parent->installEventFilter(this);
        setGeometry(parent->rect());
    }
    connect(&m_timer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    hide();
}

bool QrAnimation::eventFilter(QObject *object, QEvent *event)
{
    if (object == parent() && event->type() == QEvent::Resize)
        setGeometry(parentWidget()->rect());
    else if (object == parent() && event->type() == QEvent::Show && !m_modules.isNull())
        replay();
    return QWidget::eventFilter(object, event);
}

void QrAnimation::setCode(const QImage &modules)
{
    if (modules == m_modules)
        return;
    m_modules = modules;
    m_stitches.clear();
    QRandomGenerator random(7);
    const int n = modules.width();
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            if (qGray(modules.pixel(x, y)) < 128) {
                const double angle = random.generateDouble() * 6.2832;
                // Couture ligne par ligne, en aller-retour comme une aiguille.
                const double along = (y % 2 ? n - x : x) / double(n);
                m_stitches.append({QPoint(x, y), QPointF(std::cos(angle), std::sin(angle)) * (0.15 + 0.35 * random.generateDouble()),
                                   0.72 * (y + along) / n});
            }
    render();
    show();
    replay();
}

void QrAnimation::clear()
{
    m_modules = QImage();
    m_timer.stop();
    hide();
}

void QrAnimation::replay()
{
    if (m_modules.isNull())
        return;
    m_clock.start();
    m_timer.start(Motion::Frame);
}

void QrAnimation::succeed(const QString &text)
{
    m_result = Ok;
    m_text = text;
    m_resultClock.start();
    m_timer.start(Motion::Frame);
}

void QrAnimation::refuse()
{
    m_result = Refused;
    m_resultClock.start();
    m_timer.start(Motion::Frame);
}

void QrAnimation::resizeEvent(QResizeEvent *)
{
    render();
}

void QrAnimation::render()
{
    if (m_modules.isNull() || width() < 60)
        return;
    const double ratio = devicePixelRatioF();
    const int scale = std::max(1, int((std::min(width(), height()) - 2 * Margin) * ratio) / m_modules.width());
    m_final = QPixmap::fromImage(m_modules.scaled(m_modules.size() * scale, Qt::IgnoreAspectRatio, Qt::FastTransformation));
    m_final.setDevicePixelRatio(ratio);
}

QRectF QrAnimation::codeRect() const
{
    const QSizeF size = m_final.deviceIndependentSize();
    const double ratio = devicePixelRatioF();
    // Aligné sur les pixels de l'écran pour que les modules restent nets.
    const QPointF global = mapTo(window(), QPointF((width() - size.width()) / 2, (height() - size.height()) / 2));
    const QPointF snapped(std::round(global.x() * ratio) / ratio, std::round(global.y() * ratio) / ratio);
    return QRectF(mapFrom(window(), snapped), size);
}

void QrAnimation::paintEvent(QPaintEvent *)
{
    if (m_final.isNull())
        return;
    QPainter p(this);
    p.fillRect(rect(), QColor("#232326"));
    const double ms = m_clock.isValid() ? m_clock.elapsed() : 1e9;
    const double result = m_result == None ? 1e9 : m_resultClock.elapsed();
    const QColor gold("#e2cf9f");
    QColor tone = gold;
    if (result < Result_)
        tone = QColor(m_result == Ok ? "#7fc08a" : "#e08f8f");
    if (m_result == Refused && result < 420)
        p.translate(std::sin(result / 17) * 6 * (1 - result / 420), 0);

    const QRectF code = codeRect();
    if (ms >= Sew) {
        p.drawPixmap(code.topLeft(), m_final);
    } else {
        p.setOpacity(Motion::soft(ms / 250));
        p.fillRect(code, Qt::white);
        p.setOpacity(1);
        p.setPen(Qt::NoPen);
        const double cell = code.width() / m_modules.width(), t = ms / Sew;
        QPointF needle;
        bool sewing = false;
        for (const Stitch &s : std::as_const(m_stitches)) {
            const double go = Motion::part(t, s.delay, s.delay + 0.28);
            if (go <= 0)
                break;                       // les points sont dans l'ordre de couture
            const double e = Motion::soft(go);
            const QPointF home = code.topLeft() + QPointF(s.cell) * cell;
            const QPointF at = home + s.from * code.width() * (1 - e);
            const QColor c(int(201 * (1 - e)), int(178 * (1 - e)), int(124 * (1 - e)));
            p.setOpacity(std::min(1.0, go * 3));
            p.fillRect(QRectF(at, QSizeF(cell, cell)), c);
            needle = home;
            sewing = true;
        }
        p.setOpacity(1);
        if (sewing && t < 0.78) {            // l'aiguille
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(QColor("#8f7434"), 3, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(needle, needle + QPointF(20, -20));
            p.setPen(QPen(gold, 1.4));
            p.drawEllipse(needle + QPointF(22, -22), 2.4, 2.4);
        }
    }
    p.setRenderHint(QPainter::Antialiasing);
    const double scan = Motion::part(ms, Sew, Sew + Scan);
    if (scan > 0 && scan < 1) {              // un seul passage, puis le QR reste dégagé
        const double y = code.top() + code.height() * Motion::both(scan);
        QLinearGradient light(0, y - 34, 0, y);
        light.setColorAt(0, QColor(201, 178, 124, 0));
        light.setColorAt(1, QColor(201, 178, 124, 130));
        p.fillRect(QRectF(code.left(), std::max(code.top(), y - 34), code.width(), std::min(34.0, y - code.top())), light);
        p.setPen(QPen(QColor("#c9b27c"), 2));
        p.drawLine(QPointF(code.left(), y), QPointF(code.right(), y));
    }
    // Coins dorés, hors du QR.
    const double breath = ms < Sew + Scan ? 2 * std::sin(ms / 160) : 0;
    const QRectF f = code.adjusted(-7 - breath, -7 - breath, 7 + breath, 7 + breath);
    const double l = 26;
    p.setPen(QPen(tone, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 4; ++i) {
        const QPointF c(i % 2 ? f.right() : f.left(), i / 2 ? f.bottom() : f.top());
        const double dx = i % 2 ? -l : l, dy = i / 2 ? -l : l;
        p.drawPolyline(QPolygonF({c + QPointF(dx, 0), c, c + QPointF(0, dy)}));
    }
    if (m_result == Ok && result < Result_) {
        const double in = Motion::soft(result / 320), out = 1 - Motion::both(Motion::part(result, Result_ - 350, Result_));
        p.setOpacity(out);
        const QPointF c = code.center();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(27, 27, 29, 235));
        p.drawEllipse(c, 62 * in, 62 * in);
        p.setBrush(Qt::NoBrush);
        for (int i = 0; i < 2; ++i) {
            const double wave = Motion::part(result, 120 + i * 160, 900 + i * 160);
            QColor ring = tone;
            ring.setAlphaF(0.7 * (1 - wave));
            p.setPen(QPen(ring, 2));
            if (wave > 0) p.drawEllipse(c, 62 + 46 * wave, 62 + 46 * wave);
        }
        QPainterPath check(c + QPointF(-20, -6));
        check.lineTo(c + QPointF(-5, 9));
        check.lineTo(c + QPointF(22, -20));
        QPen pen(tone, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin);
        const double length = check.length() / 5;
        pen.setDashPattern({std::max(0.01, length * Motion::soft((result - 180) / 380)), length * 2});
        p.setPen(pen);
        p.drawPath(check);
        p.setPen(tone);
        QFont font = p.font();
        font.setBold(true);
        p.setFont(font);
        p.drawText(QRectF(c.x() - 60, c.y() + 16, 120, 24), Qt::AlignCenter, m_text);
    }
    if (result >= Result_)
        m_result = None;
    if (ms > Sew + Scan && m_result == None)
        m_timer.stop();
}
