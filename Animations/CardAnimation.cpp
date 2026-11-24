#include "CardAnimation.h"
#include "AnimationSpeed.h"
#include <QEvent>
#include <QPainter>
#include <QPainterPath>

void PopIn::play(QWidget *target, int delay)
{
    if (!target->isVisible() || target->width() < 4)
        return;
    const QString name = "popIn_" + target->objectName();
    if (QWidget *old = target->parentWidget()->findChild<QWidget *>(name, Qt::FindDirectChildrenOnly))
        delete old;
    (new PopIn(target, delay))->setObjectName(name);
}

PopIn::PopIn(QWidget *target, int delay) : QWidget(target->parentWidget()), m_home(target->geometry()), m_delay(delay)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    // Photo sur fond transparent, puis la couleur du bloc parent derrière.
    m_shot = QPixmap(target->size() * devicePixelRatioF());
    m_shot.setDevicePixelRatio(devicePixelRatioF());
    m_shot.fill(Qt::transparent);
    target->render(&m_shot, QPoint(), QRegion(), QWidget::DrawChildren);
    for (QWidget *w = target->parentWidget(); w; w = w->parentWidget())
        if (w->property("card").toBool())
            m_back = QColor("#232326");
    setGeometry(m_home.adjusted(-10, -10, 10, 34));
    show();
    raise();
    m_clock.start();
    connect(&m_timer, &QTimer::timeout, this, [this] {
        update();
        if (m_clock.elapsed() > m_delay + 620) {
            m_timer.stop();
            deleteLater();
        }
    });
    m_timer.start(Motion::frame(this));
}

void PopIn::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    const double t = Motion::part(m_clock.elapsed(), m_delay, m_delay + 560);
    const QRectF home(m_home.translated(-pos()));
    p.fillRect(home, m_back);
    if (t <= 0)
        return;
    const double s = Motion::mix(0.86, 1, Motion::spring(t)), y = 26 * (1 - Motion::soft(t));
    p.translate(home.center() + QPointF(0, y));
    p.scale(s, s);
    p.setOpacity(Motion::part(t, 0, 0.45));
    p.drawPixmap(QRectF(-home.width() / 2, -home.height() / 2, home.width(), home.height()), m_shot, QRectF(m_shot.rect()));
}

CardGlow::CardGlow(QWidget *card) : QWidget(card)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setGeometry(card->rect());
    card->installEventFilter(this);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        m_level += (m_target - m_level) * 0.18;
        if (std::abs(m_target - m_level) < 0.01)
            m_level = m_target;
        if (m_level == m_target && (m_target == 0 || m_clock.elapsed() > 1100))
            m_timer.stop();
        update();
    });
}

bool CardGlow::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::Resize)
        setGeometry(static_cast<QWidget *>(object)->rect());
    else if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
        m_target = event->type() == QEvent::Enter ? 1 : 0;
        if (m_target > 0)
            m_clock.start();
        raise();
        m_timer.start(Motion::frame(this));
    }
    return false;
}

void CardGlow::paintEvent(QPaintEvent *)
{
    if (m_level <= 0)
        return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, 12, 12);
    for (int i = 3; i > 0; --i) {
        QColor halo(226, 207, 159);
        halo.setAlphaF(0.06 * m_level);
        p.setPen(QPen(halo, i * 3));
        p.drawPath(path);
    }
    QColor edge(226, 207, 159);
    edge.setAlphaF(0.75 * m_level);
    p.setPen(QPen(edge, 1.2));
    p.drawPath(path);
    const double x = Motion::part(m_clock.elapsed(), 0, 900);
    if (x < 1) {
        p.setClipPath(path);
        const double at = -80 + (width() + 160) * Motion::both(x);
        QLinearGradient shine(at - 70, 0, at + 70, 40);
        shine.setColorAt(0, QColor(255, 246, 220, 0));
        shine.setColorAt(0.5, QColor(255, 246, 220, 30));
        shine.setColorAt(1, QColor(255, 246, 220, 0));
        p.fillRect(rect(), shine);
    }
}
