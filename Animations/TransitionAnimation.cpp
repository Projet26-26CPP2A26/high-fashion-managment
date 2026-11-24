#include "TransitionAnimation.h"
#include "AnimationSpeed.h"
#include "LogoAnimation.h"
#include <QEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>

namespace {
const double Leave = 1350, End = 2150;
}

TransitionAnimation::TransitionAnimation(QWidget *window, QLabel *logo, const QString &title, const QString &subtitle)
    : QWidget(window), m_logo(logo), m_title(title), m_subtitle(subtitle), m_menuLogo(logo->pixmap())
{
    // Opaque : Qt n'a pas à redessiner toute la fenêtre à chaque image.
    setAttribute(Qt::WA_OpaquePaintEvent);
    // Le logo du menu est vidé avant la photo : il n'y en a jamais deux.
    QPixmap empty = m_menuLogo;
    empty.fill(Qt::transparent);
    m_logo->setPixmap(empty);
    m_shot = window->grab();
    setGeometry(window->rect());
    window->installEventFilter(this);
    m_big = QPixmap(":/assets/logo_aurea.png").scaled(QSize(190, 190) * devicePixelRatioF(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
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

TransitionAnimation::~TransitionAnimation()
{
    restoreLogo();
}

void TransitionAnimation::restoreLogo()
{
    if (m_logo && !m_menuLogo.isNull())
        m_logo->setPixmap(m_menuLogo);
    m_menuLogo = QPixmap();
}

bool TransitionAnimation::eventFilter(QObject *object, QEvent *event)
{
    if (object == parent() && event->type() == QEvent::Resize) {
        setGeometry(parentWidget()->rect());
        hide();
        m_shot = parentWidget()->grab();
        show();
        raise();
    }
    return QWidget::eventFilter(object, event);
}

void TransitionAnimation::finish()
{
    if (m_done)
        return;
    m_done = true;
    m_timer.stop();
    restoreLogo();
    hide();
    emit finished();
    deleteLater();
}

void TransitionAnimation::paintEvent(QPaintEvent *)
{
    if (!m_logo)
        return;
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    const double ms = m_clock.elapsed();
    const double leave = Motion::both(Motion::part(ms, Leave, End - 60));
    const QPointF center(width() / 2.0, height() / 2.0 - 34);

    if (m_back.deviceIndependentSize().toSize() != size()) {
        m_back = QPixmap(size() * devicePixelRatioF());
        m_back.setDevicePixelRatio(devicePixelRatioF());
        QPainter b(&m_back);
        QRadialGradient back(center, std::hypot(width(), height()) * 0.55);
        back.setColorAt(0, QColor("#232220"));
        back.setColorAt(0.55, QColor("#18181a"));
        back.setColorAt(1, QColor("#111113"));
        b.fillRect(rect(), back);
    }
    p.drawPixmap(0, 0, m_back);

    // L'application arrive du fond : elle grandit et devient nette.
    const double come = Motion::soft(Motion::part(ms, Leave - 60, End - 120));
    if (come > 0) {
        const double zoom = Motion::mix(0.90, 1, come);
        const QSizeF card(width() * zoom, height() * zoom);
        const QRectF at(QPointF((width() - card.width()) / 2, (height() - card.height()) / 2), card);
        QPainterPath clip;
        clip.addRoundedRect(at, 22 * (1 - come), 22 * (1 - come));
        p.save();
        p.setOpacity(Motion::part(ms, Leave - 60, Leave + 340));
        p.setClipPath(clip);
        p.drawPixmap(at, m_shot, QRectF(m_shot.rect()));
        p.restore();
    }

    // Lumière unique, qui s'ouvre quand le logo surgit puis s'éteint.
    const double flash = Motion::part(ms, 160, 900);
    const double fade = 1 - Motion::part(ms, Leave - 150, Leave + 350);
    if (fade > 0) {
        QRadialGradient light(center, 250);
        light.setColorAt(0, QColor(226, 207, 159, int(64 * Motion::soft(flash) * fade)));
        light.setColorAt(1, QColor(226, 207, 159, 0));
        p.fillRect(QRectF(center - QPointF(250, 250), QSizeF(500, 500)), light);
    }
    if (flash > 0 && flash < 1) {
        QColor ring(241, 225, 180);
        ring.setAlphaF(0.5 * std::pow(1 - flash, 2));
        p.setPen(QPen(ring, 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(center, 70 + 130 * Motion::soft(flash), 70 + 130 * Motion::soft(flash));
    }

    // Le logo : il vient de loin, dépasse un peu sa taille, se stabilise, puis rejoint le menu.
    const double in = Motion::part(ms, 0, 820);
    const double size = 190 * std::max(0.0, Motion::mix(0.12, 1, Motion::spring(in)));
    const double ratio = m_big.width() ? double(m_big.height()) / m_big.width() : 1;
    const QRectF from(center.x() - size / 2, center.y() - size * ratio / 2, size, size * ratio);
    const QRectF to = LivingLogo::place(m_logo, window());
    const QRectF at(Motion::mix(from.x(), to.x(), leave), Motion::mix(from.y(), to.y(), leave),
                    Motion::mix(from.width(), to.width(), leave), Motion::mix(from.height(), to.height(), leave));
    const double depth = (1 - Motion::soft(in)) * (1 - leave);
    for (int i = 3; i > 0 && depth > 0.02; --i) {      // profondeur : échos qui se rejoignent
        const double k = 1 + 0.16 * i * depth;
        p.setOpacity(0.16 * depth * Motion::part(ms, 40, 220));
        p.drawPixmap(QRectF(at.center() - QPointF(at.width() * k / 2, at.height() * k / 2), at.size() * k), m_big, QRectF(m_big.rect()));
    }
    p.setOpacity(Motion::part(ms, 0, 240));
    p.drawPixmap(at, m_big, QRectF(m_big.rect()));
    p.setOpacity(1);

    const double words = Motion::soft(Motion::part(ms, 480, 980)), gone = 1 - Motion::part(ms, Leave - 120, Leave + 160);
    if (words * gone <= 0)
        return;
    p.setOpacity(words * gone);
    QFont title("Cormorant Garamond");
    title.setPixelSize(46);
    title.setWeight(QFont::DemiBold);
    title.setLetterSpacing(QFont::AbsoluteSpacing, 8);
    p.setFont(title);
    p.setPen(QColor("#e2cf9f"));
    p.drawText(QRectF(4, center.y() + 112 + 18 * (1 - words), width(), 58), Qt::AlignCenter, m_title);
    QFont small("Segoe UI");
    small.setPixelSize(14);
    p.setFont(small);
    p.setPen(QColor("#b9b4a8"));
    p.drawText(QRectF(0, center.y() + 172 + 18 * (1 - words), width(), 26), Qt::AlignCenter, m_subtitle);
}
