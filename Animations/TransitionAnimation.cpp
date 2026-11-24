#include "TransitionAnimation.h"
#include "AnimationSpeed.h"
#include "LinesAnimation.h"
#include <QEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>

namespace {
const double Gather = 1150, Leave = 2050, End = 2950;
}

TransitionAnimation::TransitionAnimation(QWidget *window, QLabel *logo, const QString &title, const QString &subtitle)
    : QWidget(window), m_logo(logo), m_title(title), m_subtitle(subtitle), m_shot(window->grab())
{
    // Opaque : Qt n'a pas à redessiner toute la fenêtre à chaque image.
    setAttribute(Qt::WA_OpaquePaintEvent);
    setGeometry(window->rect());
    window->installEventFilter(this);
    m_big = QPixmap(":/assets/logo_aurea.png").scaled(QSize(170, 170) * devicePixelRatioF(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        update();
        if (m_clock.elapsed() > End)
            finish();
    });
    m_timer.setTimerType(Qt::PreciseTimer);
    show();
    raise();
    m_clock.start();
    m_timer.start(Motion::frame(this));
}

bool TransitionAnimation::eventFilter(QObject *object, QEvent *event)
{
    if (object == parent() && event->type() == QEvent::Resize)
        setGeometry(parentWidget()->rect());
    return QWidget::eventFilter(object, event);
}

void TransitionAnimation::finish()
{
    if (m_done)
        return;
    m_done = true;
    m_timer.stop();
    hide();
    emit finished();
    deleteLater();
}

void TransitionAnimation::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    const double ms = m_clock.elapsed();
    const double leave = Motion::both(Motion::part(ms, Leave, End));
    const QPointF center(width() / 2.0, height() / 2.0 - 36);
    const double reach = std::hypot(width(), height()) * 0.62;

    // Fond, percé d'un cercle qui grandit et laisse voir l'application.
    const double hole = reach * Motion::both(Motion::part(ms, Leave + 120, End));
    if (m_back.deviceIndependentSize().toSize() != size()) {      // le fond n'est calculé qu'une fois
        m_back = QPixmap(size() * devicePixelRatioF());
        m_back.setDevicePixelRatio(devicePixelRatioF());
        QPainter b(&m_back);
        QRadialGradient back(center, reach);
        back.setColorAt(0, QColor("#26241f"));
        back.setColorAt(0.45, QColor("#1b1b1d"));
        back.setColorAt(1, QColor("#121214"));
        b.fillRect(rect(), back);
    }
    p.drawPixmap(0, 0, m_back);
    if (hole > 0) {
        QPainterPath circle;
        circle.addEllipse(center, hole, hole);
        p.save();
        p.setClipPath(circle);
        p.drawPixmap(0, 0, m_shot);
        p.restore();
    }
    if (hole > 0) {
        for (int i = 0; i < 4; ++i) {
            QColor rim(241, 225, 180);
            rim.setAlphaF((1 - leave) * (0.55 - 0.13 * i));
            p.setPen(QPen(rim, 1.5 + 4 * i));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(center, hole, hole);
        }
    }

    LinesAnimation::Scene lines;
    lines.area = rect();
    lines.center = center;
    lines.radius = 108 * (1 + 2.6 * leave);
    lines.time = ms / 1000;
    lines.drawn = Motion::both(Motion::part(ms, 0, 950));
    lines.gather = Motion::part(ms, 450, Gather + 250);
    lines.alpha = 1 - Motion::part(ms, Leave + 100, End - 250);
    LinesAnimation::paint(p, lines, false);

    // Logo : apparaît au centre, puis vole jusqu'à sa place dans le menu.
    const double in = Motion::soft(Motion::part(ms, 350, 1100));
    const QSizeF logo = QSizeF(m_big.size()) / devicePixelRatioF();
    QRectF from(center - QPointF(logo.width() / 2, logo.height() / 2), logo);
    const double pop = Motion::mix(0.72, 1, Motion::spring(Motion::part(ms, 350, 1100)));
    from = QRectF(center - QPointF(logo.width(), logo.height()) * pop / 2, logo * pop);
    QRectF to(m_logo->mapTo(window(), QPoint((m_logo->width() - 44) / 2, (m_logo->height() - 44) / 2)), QSizeF(44, 44));
    if (logo.width() > 0)
        to.setHeight(44 * logo.height() / logo.width());
    QRectF at(Motion::mix(from.x(), to.x(), leave), Motion::mix(from.y(), to.y(), leave) - 60 * std::sin(leave * Motion::Pi),
              Motion::mix(from.width(), to.width(), leave), Motion::mix(from.height(), to.height(), leave));
    QRadialGradient glow(at.center(), at.width() * 1.1);
    glow.setColorAt(0, QColor(226, 207, 159, int(60 * in * (1 - leave))));
    glow.setColorAt(1, QColor(226, 207, 159, 0));
    p.fillRect(at.adjusted(-at.width(), -at.width(), at.width(), at.width()), glow);
    p.setOpacity(in);
    p.drawPixmap(at, m_big, QRectF(m_big.rect()));
    const double sweep = Motion::part(ms, 1050, 1750);
    if (sweep > 0 && sweep < 1) {
        QPainterPath disc;
        disc.addEllipse(at.center(), at.width() * 0.4, at.width() * 0.4);
        p.save();
        p.setClipPath(disc);
        const double x = at.left() - at.width() * 0.4 + at.width() * 1.8 * sweep;
        QLinearGradient shine(x - 34, at.top(), x + 34, at.top() + 20);
        shine.setColorAt(0, QColor(255, 246, 214, 0));
        shine.setColorAt(0.5, QColor(255, 246, 214, 130));
        shine.setColorAt(1, QColor(255, 246, 214, 0));
        p.fillRect(at, shine);
        p.restore();
    }
    p.setOpacity(1);
    LinesAnimation::paint(p, lines, true);

    // Titre, lettre par lettre.
    QFont title("Cormorant Garamond");
    title.setPixelSize(46);
    p.setFont(title);
    const double gap = 9, out = 1 - Motion::part(ms, Leave - 120, Leave + 260);
    double total = 0;
    for (const QChar c : m_title)
        total += p.fontMetrics().horizontalAdvance(c) + gap;
    double x = width() / 2.0 - (total - gap) / 2;
    for (int i = 0; i < m_title.size(); ++i) {
        const double show = Motion::soft(Motion::part(ms, 850 + 45 * i, 1350 + 45 * i));
        QColor gold("#e9d8a8");
        gold.setAlphaF(show * out);
        p.setPen(gold);
        p.drawText(QPointF(x, center.y() + 196 + 16 * (1 - show)), QString(m_title[i]));
        x += p.fontMetrics().horizontalAdvance(m_title[i]) + gap;
    }
    QFont small("Inter");
    small.setPixelSize(14);
    p.setFont(small);
    QColor grey("#b9b4a8");
    grey.setAlphaF(Motion::soft(Motion::part(ms, 1250, 1700)) * out);
    p.setPen(grey);
    p.drawText(QRectF(0, center.y() + 214, width(), 26), Qt::AlignCenter, m_subtitle);
}
