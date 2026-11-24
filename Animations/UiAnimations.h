#pragma once
#include "AnimationSpeed.h"

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QLabel>
#include <QPropertyAnimation>
#include <QStackedWidget>
#include <QTimer>
#include <QVariantAnimation>
#include <QVector>
#include <QWidget>
#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif

namespace Fx {
inline int frameInterval(const QWidget *w)
{
    const QScreen *screen = w->screen();
    const double hz = screen && screen->refreshRate() > 20 ? screen->refreshRate() : 60;
    return std::clamp(int(1000 / hz), 5, 33);
}

inline void steady(QWidget *w)
{
#ifdef Q_OS_WIN
    const HWND handle = reinterpret_cast<HWND>(w->winId());
    const BOOL on = TRUE;
    const COLORREF none = 0xFFFFFFFE, caption = RGB(21, 21, 23);
    DwmSetWindowAttribute(handle, 3, &on, sizeof(on));
    DwmSetWindowAttribute(handle, 20, &on, sizeof(on));
    DwmSetWindowAttribute(handle, 34, &none, sizeof(none));
    if (!w->windowFlags().testFlag(Qt::FramelessWindowHint)) DwmSetWindowAttribute(handle, 35, &caption, sizeof(caption));
#else
    Q_UNUSED(w);
#endif
}

inline void openWindow(QWidget *w)
{
    w->setWindowOpacity(0);
    auto *a = new QPropertyAnimation(w, "windowOpacity", w);
    a->setDuration(Motion::Base);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setEasingCurve(QEasingCurve::OutCubic);
    QTimer::singleShot(0, a, [a] { a->start(QAbstractAnimation::DeleteWhenStopped); });
}

inline void shake(QWidget *w)
{
    if (w->property("shaking").toBool()) return;
    w->setProperty("shaking", true);
    const QPoint home = w->pos();
    auto *a = new QPropertyAnimation(w, "pos", w);
    a->setDuration(Motion::Slow);
    for (int i = 0; i <= 6; ++i) a->setKeyValueAt(i / 6.0, home + QPoint(i == 0 || i == 6 ? 0 : (i % 2 ? 6 : -6), 0));
    QObject::connect(a, &QAbstractAnimation::finished, w, [w] { w->setProperty("shaking", false); });
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

template <class Pages>
void switchPage(Pages *stack, int index)
{
    if (stack->currentIndex() == index) return;

    const int way = index > stack->currentIndex() ? -1 : 1;
    if (!stack->isVisible()) { stack->setCurrentIndex(index); return; }
    auto *ghost = new QLabel(stack);
    ghost->setPixmap(stack->window()->grab(QRect(stack->mapTo(stack->window(), QPoint()), stack->size())));
    ghost->setGeometry(stack->rect());
    stack->setCurrentIndex(index);
    ghost->setAttribute(Qt::WA_TransparentForMouseEvents);
    ghost->show();
    ghost->raise();
    auto *effect = new QGraphicsOpacityEffect(ghost);
    ghost->setGraphicsEffect(effect);
    auto *a = new QVariantAnimation(ghost);
    a->setDuration(Motion::Base);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(a, &QVariantAnimation::valueChanged, ghost, [ghost, effect, way](const QVariant &v) {
        effect->setOpacity(1 - v.toReal());
        ghost->move(int(way * 16 * v.toReal()), 0);
    });
    QObject::connect(a, &QAbstractAnimation::finished, ghost, &QObject::deleteLater);
    a->start();
}

inline void countTo(QLabel *label, int value)
{
    const int from = label->property("count").toInt();
    if (from == value) return;
    label->setProperty("count", value);
    auto *a = new QVariantAnimation(label);
    a->setDuration(Motion::Slow);
    a->setStartValue(from);
    a->setEndValue(value);
    a->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(a, &QVariantAnimation::valueChanged, label, [label, value](const QVariant &v) {
        if (label->property("count").toInt() == value) label->setText(QString::number(v.toInt()));
    });
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

inline void toast(QWidget *window, const QString &text)
{
    if (QLabel *old = window->findChild<QLabel *>("fxToast", Qt::FindDirectChildrenOnly)) { old->setObjectName(QString()); old->hide(); old->deleteLater(); }
    auto *t = new QLabel(text, window);
    t->setObjectName("fxToast");
    t->setTextFormat(Qt::PlainText);
    t->setAttribute(Qt::WA_TransparentForMouseEvents);
    t->setStyleSheet("background:#2e2a20;border:1px solid #c9b27c;border-radius:10px;color:#f4e7bc;padding:12px 18px;font-weight:600;");
    t->adjustSize();
    const QPoint end(window->width() - t->width() - 26, window->height() - t->height() - 26);
    t->move(end + QPoint(0, 16));
    t->show();
    t->raise();
    auto *effect = new QGraphicsOpacityEffect(t);
    effect->setOpacity(0);
    t->setGraphicsEffect(effect);
    const auto play = [t, effect, end](double from, double to) {
        auto *a = new QVariantAnimation(t);
        a->setDuration(Motion::Base);
        a->setStartValue(from);
        a->setEndValue(to);
        a->setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(a, &QVariantAnimation::valueChanged, t, [t, effect, end](const QVariant &v) {
            effect->setOpacity(v.toReal());
            t->move(end + QPoint(0, int(16 * (1 - v.toReal()))));
        });
        a->start(QAbstractAnimation::DeleteWhenStopped);
        return a;
    };
    play(0, 1);
    QTimer::singleShot(3600, t, [t, play] { QObject::connect(play(1, 0), &QAbstractAnimation::finished, t, &QObject::deleteLater); });
}

inline void navIndicator(QWidget *sidebar, const QVector<QPushButton *> &buttons)
{
    auto *pill = new QFrame(sidebar);
    pill->setAttribute(Qt::WA_TransparentForMouseEvents);
    pill->setStyleSheet("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #bfa370,stop:0.5 #f1e1b4,stop:1 #c4a876);border-radius:9px;");
    auto *a = new QPropertyAnimation(pill, "geometry", pill);
    a->setDuration(Motion::Base);
    a->setEasingCurve(QEasingCurve::OutCubic);
    for (QPushButton *b : buttons) {
        b->setStyleSheet("QPushButton:checked{background:transparent;}");
        b->raise();
        QObject::connect(b, &QPushButton::toggled, pill, [pill, a, b](bool on) {
            if (!on || !pill->isVisible()) return;
            a->stop();
            a->setStartValue(pill->geometry());
            a->setEndValue(b->geometry());
            a->start();
        });
    }
    auto *keep = new QTimer(pill);
    QObject::connect(keep, &QTimer::timeout, pill, [pill, a, buttons, sidebar] {
        if (a->state() == QAbstractAnimation::Running || !sidebar->isVisible()) return;
        for (QPushButton *b : buttons) if (b->isChecked() && pill->geometry() != b->geometry()) pill->setGeometry(b->geometry());
        pill->show();
    });
    keep->start(60);
    pill->hide();
}

class Glow : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *o, QEvent *e) override
    {
        auto *b = qobject_cast<QPushButton *>(o);
        if (b && (e->type() == QEvent::Enter || e->type() == QEvent::Leave) && !b->property("nav").toBool()) {
            const bool in = e->type() == QEvent::Enter && b->isEnabled();
            auto *effect = qobject_cast<QGraphicsDropShadowEffect *>(b->graphicsEffect());
            if (!effect && in) {
                effect = new QGraphicsDropShadowEffect(b);
                effect->setOffset(0, 0);
                effect->setColor(QColor(226, 207, 159, 110));
                effect->setBlurRadius(0);
                b->setGraphicsEffect(effect);
            }
            if (effect) {
                auto *a = new QPropertyAnimation(effect, "blurRadius", effect);
                a->setDuration(Motion::Quick);
                a->setEasingCurve(QEasingCurve::OutCubic);
                a->setEndValue(in ? 12.0 : 0.0);
                a->start(QAbstractAnimation::DeleteWhenStopped);
            }
        }
        return QObject::eventFilter(o, e);
    }
};
inline void glowButtons(QWidget *)
{
    static QPointer<Glow> glow;
    if (glow) return;
    glow = new Glow(qApp);
    qApp->installEventFilter(glow);
}

inline void fullScreenKey(QWidget *window)
{
    QObject::connect(new QShortcut(QKeySequence(Qt::Key_F11), window), &QShortcut::activated, window, [window] {
        if (!window->isFullScreen()) {
            window->setProperty("wasMaximized", window->isMaximized());
            window->showFullScreen();
        } else if (window->property("wasMaximized").toBool()) window->showMaximized();
        else window->showNormal();
    });
}

inline void reveal(QWidget *w, int delay, int duration = Motion::Slow)
{
    auto *effect = new QGraphicsOpacityEffect(w);
    effect->setOpacity(0);
    w->setGraphicsEffect(effect);
    auto *a = new QPropertyAnimation(effect, "opacity", effect);
    a->setDuration(duration);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(a, &QAbstractAnimation::finished, w, [w, effect] { if (w->graphicsEffect() == effect) w->setGraphicsEffect(nullptr); }, Qt::QueuedConnection);
    QTimer::singleShot(delay, a, [a] { a->start(); });
}

}
