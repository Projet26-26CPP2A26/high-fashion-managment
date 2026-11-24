#include "TransitionAnimation.h"
#include "UiAnimations.h"
#include "ClosingAnimation.h"
#include <QEventLoop>
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace {
QPointer<MorphOverlay> current;

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }
double outCubic(double v) { return 1 - std::pow(1 - clamp01(v), 3); }
double inOut(double v)
{
    v = clamp01(v);
    return v < 0.5 ? 4 * v * v * v : 1 - std::pow(-2 * v + 2, 3) / 2;
}
QRectF mix(const QRectF &a, const QRectF &b, double t)
{
    return QRectF(a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t, a.width() + (b.width() - a.width()) * t, a.height() + (b.height() - a.height()) * t);
}
}

QRect MorphOverlay::areaFor(QWidget *window)
{
    const QScreen *screen = window && window->screen() ? window->screen() : QGuiApplication::primaryScreen();
    return window && window->isFullScreen() ? screen->geometry() : screen->availableGeometry();
}

MorphOverlay::MorphOverlay(QWidget *from, const QRect &logo, const QRect &target, Spectacle::Mode mode, const LogoSwitch &logoVisible)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool), m_mode(mode)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setGeometry(areaFor(from));
    const QPoint origin = geometry().topLeft();
    logoVisible(false);
    m_shot = from->grab();
    if (mode == Spectacle::Closing) logoVisible(true);
    if (mode == Spectacle::Closing && ClosingView::available()) {
        ClosingView::Scene scene;
        scene.area = geometry();
        scene.window = QRectF(QRect(from->mapToGlobal(QPoint(0, 0)), from->size()));
        scene.logo = QRectF(logo);
        scene.shot = m_shot.toImage();
        scene.mark = logos().last().toImage();
        if (from->property("ringTime").isValid()) scene.ringTime = from->property("ringTime").toDouble();
        m_view.reset(new ClosingView(scene));
        if (m_view->begin()) return;
        m_view.reset();
    }
    if (mode == Spectacle::Closing) logoVisible(false);
    if (mode == Spectacle::Closing) {
        m_dim = m_shot;
        QPainter shade(&m_dim);
        shade.fillRect(QRectF(QPointF(0, 0), QSizeF(from->size())), QColor(15, 15, 16, 178));
        shade.end();
        m_soft = m_dim.toImage().scaled((QSizeF(from->size()) / 3).toSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        m_soft.setDevicePixelRatio(1);
        m_feather = m_soft;
        QPainter f(&m_feather);
        f.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        f.translate(m_feather.width() / 2.0, m_feather.height() / 2.0);
        f.scale(m_feather.width() / 2.0, m_feather.height() / 2.0);
        QRadialGradient edge(QPointF(0, 0), 1);
        edge.setColorAt(0, QColor(0, 0, 0, 255));
        edge.setColorAt(0.45, QColor(0, 0, 0, 225));
        edge.setColorAt(0.75, QColor(0, 0, 0, 95));
        edge.setColorAt(1, QColor(0, 0, 0, 0));
        f.fillRect(QRectF(-1, -1, 2, 2), edge);
    }
    m_start = QRectF(QRect(from->mapToGlobal(QPoint(0, 0)), from->size()).translated(-origin));
    m_target = QRectF(target.translated(-origin));
    m_logoStart = QRectF(logo.translated(-origin));
    auto *timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, this, [this] {
        advance();
        update();
    });
    timer->start(Fx::frameInterval(this));
    current = this;
    Fx::steady(this);
    show();
    raise();
    activateWindow();
    m_clock.start();
    repaint();
}

MorphOverlay::~MorphOverlay() = default;

const QVector<QPixmap> &MorphOverlay::logos()
{
    static QVector<QPixmap> levels;
    if (levels.isEmpty()) {
        const QImage source(":/assets/logo_aurea.png");
        const qreal ratio = qApp->devicePixelRatio();
        for (const int size : {64, 70, 96, 124, 150, 180, 240}) levels.append(QPixmap::fromImage(source.scaled(QSize(size, size) * ratio, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }
    return levels;
}

void MorphOverlay::warmUp()
{
    logos();
    QImage surface(64, 64, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&surface);
    for (const double size : {10.0, 13.0, 19.0, 30.0}) {
        QFont f("Segoe UI");
        f.setPointSizeF(size);
        f.setWeight(size > 15 ? QFont::Bold : QFont::Normal);
        p.setFont(f);
        p.drawText(0, 40, QStringLiteral("ÀBbienvôtRouj,0123456789"));
    }
}

void MorphOverlay::advance()
{
    const qint64 now = m_clock.nsecsElapsed();
    if (m_lastFrame >= 0) m_time += std::min((now - m_lastFrame) / 1e9, 0.034);
    m_lastFrame = now;
}

void MorphOverlay::breathe()
{
    if (!current || !current->isVisible()) return;
    if (current->m_clock.nsecsElapsed() - current->m_lastFrame < 12000000) return;
    current->advance();
    current->repaint();
}

void MorphOverlay::waitClosed()
{
    if (m_view) {
        wait([this] { return m_view->over(); });
        m_view->hide();
        close();
        return;
    }
    wait([this] { return m_skip || seconds() >= Close; });
    close();
}

void MorphOverlay::setMessage(const QString &title, const QString &subtitle, double duration)
{
    if (m_view) m_view->setMessage(title, subtitle);
    m_duration = duration;
    m_title = title;
    m_subtitle = subtitle;
    m_messageAt = std::max(seconds(), Expand * 0.40);
    m_skip = false;
}

bool MorphOverlay::messageOver() const { return m_messageAt < 0 || m_skip || seconds() >= m_messageAt + m_duration; }

void MorphOverlay::wait(const std::function<bool()> &done)
{
    if (done()) return;
    QEventLoop loop;
    QTimer check;
    check.setTimerType(Qt::PreciseTimer);
    connect(&check, &QTimer::timeout, &loop, [&] { if (done()) loop.quit(); });
    check.start(4);
    loop.exec();
}

void MorphOverlay::waitExpanded() { wait([this] { return seconds() >= Expand; }); }
void MorphOverlay::waitMessage() { wait([this] { return messageOver(); }); }

void MorphOverlay::drawLogo(QPainter &p, const QRectF &rect) const
{
    const double need = rect.width() * devicePixelRatioF() - 0.6;
    for (const QPixmap &level : logos()) {
        if (level.width() < need && &level != &logos().last()) continue;
        p.drawPixmap(rect, level, QRectF(level.rect()));
        return;
    }
}

void MorphOverlay::land(std::function<QRect()> logo, LogoSwitch logoVisible)
{
    logoVisible(false);
    auto *timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, this, [this, timer, logo, logoVisible] {
        if (m_landAt < 0) {
            if (!messageOver()) return;
            m_logoEnd = QRectF(logo().translated(-geometry().topLeft()));
            m_landAt = seconds();
            raise();
        } else if (seconds() >= m_landAt + Land) {
            timer->stop();
            logoVisible(true);
            QTimer::singleShot(50, this, &QWidget::close);
        }
    });
    timer->start(4);
}

void MorphOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    if (m_mode == Spectacle::Closing) {
        paintClosing(p);
        return;
    }
    const double now = seconds(), grow = inOut(now / Expand);
    const double land = m_landAt < 0 ? 0 : clamp01((now - m_landAt) / Land), leave = inOut(land);

    const QRectF shape = mix(m_start, m_target, grow);
    const double corner = (m_target.width() > m_start.width() ? 10 * (1 - grow) : 8 * grow) + 10 * std::sin(M_PI * grow);
    QPainterPath clip;
    clip.addRoundedRect(shape, corner, corner);
    p.setOpacity(1 - outCubic((land - 0.15) / 0.85));
    QLinearGradient back(shape.topLeft(), shape.bottomRight());
    back.setColorAt(0, QColor("#141416"));
    back.setColorAt(0.65, QColor("#1b1a17"));
    back.setColorAt(1, QColor("#2c2619"));
    p.fillPath(clip, back);
    if (now < Expand * 0.45) {
        p.save();
        p.setClipPath(clip);
        p.setOpacity(1 - outCubic(now / (Expand * 0.45)));
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.drawPixmap((shape.center() - QPointF(m_start.width() / 2, m_start.height() / 2)).toPoint(), m_shot);
        p.restore();
    }

    const QPointF center(m_target.center().x(), m_target.center().y() - 46);
    const double big = std::min({170.0, m_target.height() * 0.30, m_target.width() * 0.3});
    const QRectF middle(center.x() - big / 2, center.y() - big / 2, big, big);
    QRectF logo = mix(m_logoStart, middle, grow);
    if (land > 0) {
        logo = mix(middle, m_logoEnd, leave);
    }
    const QPointF c = logo.center();
    const double r = logo.width() / 2;
    m_show.build({m_mode, c, big, now, clamp01(now / Expand), land, QSizeF(size())});
    m_show.paint(p, false);
    const double halo = grow * (1 - leave);
    if (halo > 0.01) {
        p.setOpacity(halo);
        QRadialGradient glow(c, r * 2.4);
        glow.setColorAt(0, QColor(226, 207, 159, 44));
        glow.setColorAt(1, QColor(226, 207, 159, 0));
        p.fillRect(QRectF(c.x() - r * 2.4, c.y() - r * 2.4, r * 4.8, r * 4.8), glow);
    }
    p.setOpacity(1);
    drawLogo(p, logo);
    m_show.paint(p, true);

    if (m_messageAt < 0) return;
    const double t = now - m_messageAt, out = 1 - outCubic(land / 0.35);
    QFont big_("Segoe UI");
    big_.setPointSizeF(std::clamp(m_target.width() / 46, 16.0, 30.0));
    big_.setWeight(QFont::Bold);
    const QFontMetricsF fm(big_);
    double x = m_target.center().x() - fm.horizontalAdvance(m_title) / 2;
    const double y = center.y() + big / 2 + 78;
    p.setFont(big_);
    const double veil = outCubic(t / 0.4) * out;
    if (veil > 0.01) {
        p.setOpacity(veil);
        QRadialGradient shade(QPointF(0, 0), 1);
        shade.setColorAt(0, QColor(16, 16, 17, 215));
        shade.setColorAt(0.55, QColor(16, 16, 17, 150));
        shade.setColorAt(1, QColor(16, 16, 17, 0));
        p.save();
        p.translate(m_target.center().x(), y + 8);
        p.scale(std::max(260.0, fm.horizontalAdvance(m_title) * 0.95), 78);
        p.fillRect(QRectF(-1, -1, 2, 2), shade);
        p.restore();
    }
    const double a = outCubic(t / 0.32);
    p.setOpacity(a * out);
    p.setPen(QColor("#fff9ea"));
    p.drawText(QPointF(x, y + 8 * (1 - a)), m_title);
    QFont small("Segoe UI");
    small.setPointSizeF(std::clamp(m_target.width() / 110, 10.0, 13.0));
    p.setFont(small);
    p.setPen(QColor("#cdbd92"));
    p.drawText(QRectF(m_target.left(), y + 18 + 8 * (1 - a), m_target.width(), 30), Qt::AlignCenter, m_subtitle);
}

void MorphOverlay::paintClosing(QPainter &p)
{
    const double now = seconds();
    const double gather = inOut(now / 0.45), pull = clamp01((now - 0.55) / 0.85), absorb = clamp01((now - 1.30) / 0.28), end = clamp01((now - 1.56) / 0.36);
    const QPointF c(m_start.center().x(), m_start.center().y() - 30);
    const double big = std::min({150.0, m_start.height() * 0.28, m_start.width() * 0.28});

    if (pull <= 0) {
        p.setOpacity(1);
        p.drawPixmap(m_start.topLeft(), m_shot);
        p.setOpacity(gather);
        p.drawPixmap(m_start.topLeft(), m_dim);
    } else if (pull < 1) {
        const double q = pull * pull * (1.55 - 0.55 * pull);
        const QSizeF half = m_start.size() / 3;
        if (m_buffer.size() != half.toSize()) m_buffer = QImage(half.toSize(), QImage::Format_ARGB32_Premultiplied);
        m_buffer.fill(Qt::transparent);
        const QPointF eye = (c - m_start.topLeft()) / 3;
        QPainter b(&m_buffer);
        b.setRenderHint(QPainter::SmoothPixmapTransform, q > 0.02);
        const double blur = 0.30 * std::sin(M_PI * std::min(1.0, pull * 1.05));
        for (int i = 2; i >= 0; --i) {
            const double scale = (1 - 0.86 * q) * (1 + blur * i * 0.55);
            b.setOpacity(i == 0 ? 1 - 0.35 * pull : 0.30);
            b.drawImage(QRectF(eye.x() - eye.x() * scale, eye.y() - eye.y() * scale, half.width() * scale, half.height() * scale), m_feather);
        }
        const double solid = 1 - clamp01(pull / 0.34);
        if (solid > 0.01) {
            p.setOpacity(solid * solid * (3 - 2 * solid));
            p.drawPixmap(m_start.topLeft(), m_dim);
        }
        b.setOpacity(1);
        b.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        const double extent = std::hypot(std::max(eye.x(), half.width() - eye.x()), std::max(eye.y(), half.height() - eye.y()));
        const double radius = std::max(2.0, extent * 2.3 * std::pow(1 - pull, 1.25));
        QRadialGradient mask(eye, radius);
        mask.setColorAt(0, QColor(0, 0, 0, 255));
        mask.setColorAt(0.28, QColor(0, 0, 0, 245));
        mask.setColorAt(0.55, QColor(0, 0, 0, 150));
        mask.setColorAt(0.80, QColor(0, 0, 0, 45));
        mask.setColorAt(1, QColor(0, 0, 0, 0));
        b.fillRect(m_buffer.rect(), mask);
        b.end();
        p.setOpacity(1);
        p.drawImage(m_start, m_buffer);
    }

    m_show.build({Spectacle::Closing, c, big, now, clamp01((now - 0.45) / 0.60), clamp01((now - 0.90) / 0.52), QSizeF(size())});
    m_show.paint(p, false);
    m_show.paint(p, true);

    if (m_messageAt >= 0 && pull < 1) {
        const double in = outCubic((now - 0.18) / 0.30), drawn = inOut((now - 0.86) / 0.42);
        const double scale = 1 - drawn;
        const QPointF home(c.x(), c.y() + big / 2 + 62);
        p.save();
        p.translate(home.x() + (c.x() - home.x()) * drawn, home.y() + (c.y() - home.y()) * drawn + 8 * (1 - in));
        p.scale(std::max(0.01, scale), std::max(0.01, scale));
        p.setOpacity(in * (1 - clamp01((drawn - 0.6) / 0.4)));
        QFont title("Segoe UI");
        title.setPointSizeF(21);
        title.setWeight(QFont::Bold);
        p.setFont(title);
        p.setPen(QColor("#fff9ea"));
        p.drawText(QRectF(-400, -22, 800, 36), Qt::AlignCenter, m_title);
        QFont small("Segoe UI");
        small.setPointSizeF(10.5);
        p.setFont(small);
        p.setPen(QColor("#cdbd92"));
        p.drawText(QRectF(-400, 18, 800, 26), Qt::AlignCenter, m_subtitle);
        p.restore();
    }

    const double halo = gather * (0.5 + 0.9 * std::sin(M_PI * absorb)) * (1 - end);
    const double pulse = 1 + 0.09 * std::sin(M_PI * absorb);
    QRectF logo = mix(m_logoStart, QRectF(c.x() - big / 2, c.y() - big / 2, big, big), gather);
    const QPointF l = logo.center();
    const double shrink = end <= 0 ? pulse : 1 - std::pow(end, 2.6);
    logo = QRectF(l.x() - logo.width() / 2 * shrink, l.y() - logo.height() / 2 * shrink, logo.width() * shrink, logo.height() * shrink);
    if (halo > 0.01) {
        p.setOpacity(std::min(1.0, halo));
        QRadialGradient glow(l, big * 1.1);
        glow.setColorAt(0, QColor(226, 207, 159, 62));
        glow.setColorAt(1, QColor(226, 207, 159, 0));
        p.fillRect(QRectF(l.x() - big * 1.1, l.y() - big * 1.1, big * 2.2, big * 2.2), glow);
    }
    if (logo.width() > 2) {
        p.setOpacity(1 - clamp01((end - 0.75) / 0.25));
        drawLogo(p, logo);
    }
    if (end > 0 && end < 1) {
        const double radius = big * 0.72 * (1 - outCubic(end));
        p.setOpacity(std::sin(M_PI * end) * 0.8);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(241, 225, 180), 1.4));
        p.drawEllipse(l, radius, radius);
    }
}
