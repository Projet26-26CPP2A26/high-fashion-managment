#pragma once
#include "Motion.h"
#include <QEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVariantAnimation>
#include <functional>

namespace UiEffects {

// Change de page : l'ancienne page s'efface en glissant.
inline void changePage(QStackedWidget *pages, int index)
{
    if (pages->currentIndex() == index || !pages->isVisible()) {
        pages->setCurrentIndex(index);
        return;
    }
    const int direction = index > pages->currentIndex() ? -1 : 1;
    if (QLabel *old = pages->findChild<QLabel *>("pageGhost"))
        delete old;
    auto *ghost = new QLabel(pages);
    ghost->setObjectName("pageGhost");
    ghost->setAttribute(Qt::WA_TransparentForMouseEvents);
    ghost->setPixmap(pages->currentWidget()->grab());
    ghost->setGeometry(pages->rect());
    pages->setCurrentIndex(index);
    ghost->show();
    ghost->raise();
    auto *opacity = new QGraphicsOpacityEffect(ghost);
    ghost->setGraphicsEffect(opacity);
    auto *a = new QVariantAnimation(ghost);
    a->setDuration(Motion::Normal);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    QObject::connect(a, &QVariantAnimation::valueChanged, ghost, [=](const QVariant &v) {
        const double t = Motion::soft(v.toDouble());
        opacity->setOpacity(1 - t);
        ghost->move(int(direction * 28 * t), 0);
    });
    QObject::connect(a, &QAbstractAnimation::finished, ghost, &QObject::deleteLater);
    a->start();
}

// Un chiffre qui compte jusqu'à sa nouvelle valeur.
inline void count(QLabel *label, double value, const std::function<QString(double)> &text)
{
    const double from = label->property("shown").toDouble();
    label->setProperty("shown", value);
    if (QVariantAnimation *old = label->findChild<QVariantAnimation *>("count"))
        delete old;
    if (!label->isVisible() || from == value) {
        label->setText(text(value));
        return;
    }
    auto *a = new QVariantAnimation(label);
    a->setObjectName("count");
    a->setDuration(Motion::Long);
    a->setStartValue(from);
    a->setEndValue(value);
    a->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(a, &QVariantAnimation::valueChanged, label, [=](const QVariant &v) { label->setText(text(v.toDouble())); });
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

// Message doré en bas à droite.
inline void toast(QWidget *window, const QString &message)
{
    if (QLabel *old = window->findChild<QLabel *>("toast", Qt::FindDirectChildrenOnly))
        delete old;
    auto *t = new QLabel(message, window);
    t->setObjectName("toast");
    t->setTextFormat(Qt::PlainText);
    t->setAttribute(Qt::WA_TransparentForMouseEvents);
    t->setStyleSheet("background:#2b2820;border:1px solid #c9b27c;border-radius:10px;color:#f1e1b4;padding:12px 18px;font-weight:600;");
    t->adjustSize();
    auto *opacity = new QGraphicsOpacityEffect(t);
    t->setGraphicsEffect(opacity);
    t->show();
    t->raise();
    auto *a = new QVariantAnimation(t);
    a->setDuration(3600);
    a->setStartValue(0.0);
    a->setEndValue(3600.0);
    const auto place = [=](double ms) {
        const double shown = Motion::soft(ms / Motion::Normal) * (1 - Motion::both(Motion::part(ms, 3600 - Motion::Normal, 3600)));
        opacity->setOpacity(shown);
        t->move(window->width() - t->width() - 28, window->height() - t->height() - 44 + int(18 * (1 - shown)));
    };
    place(0);
    QObject::connect(a, &QVariantAnimation::valueChanged, t, [=](const QVariant &v) { place(v.toDouble()); });
    QObject::connect(a, &QAbstractAnimation::finished, t, &QObject::deleteLater);
    a->start();
}

// Petite secousse (erreur de saisie).
inline void shake(QWidget *w)
{
    if (w->property("shaking").toBool())
        return;
    w->setProperty("shaking", true);
    const QPoint home = w->pos();
    auto *a = new QVariantAnimation(w);
    a->setDuration(420);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    QObject::connect(a, &QVariantAnimation::valueChanged, w, [=](const QVariant &v) {
        const double t = v.toDouble();
        w->move(home + QPoint(int(7 * (1 - t) * std::sin(t * 25)), 0));
    });
    QObject::connect(a, &QAbstractAnimation::finished, w, [=] { w->move(home); w->setProperty("shaking", false); });
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

// Lueur dorée au survol des boutons.
class HoverGlow : public QObject {
public:
    static void install(QWidget *root)
    {
        auto *glow = new HoverGlow(root);
        const auto buttons = root->findChildren<QPushButton *>();
        for (QPushButton *b : buttons)
            if (!b->property("nav").toBool())
                b->installEventFilter(glow);
    }

protected:
    explicit HoverGlow(QObject *parent) : QObject(parent) {}
    bool eventFilter(QObject *o, QEvent *e) override
    {
        if (e->type() != QEvent::Enter && e->type() != QEvent::Leave)
            return false;
        auto *b = static_cast<QPushButton *>(o);
        auto *shadow = qobject_cast<QGraphicsDropShadowEffect *>(b->graphicsEffect());
        if (!shadow) {
            shadow = new QGraphicsDropShadowEffect(b);
            shadow->setOffset(0, 0);
            shadow->setBlurRadius(0);
            shadow->setColor(QColor(226, 207, 159, 120));
            b->setGraphicsEffect(shadow);
        }
        auto *a = new QPropertyAnimation(shadow, "blurRadius", shadow);
        a->setDuration(Motion::Fast);
        a->setEndValue(e->type() == QEvent::Enter && b->isEnabled() ? 16.0 : 0.0);
        a->start(QAbstractAnimation::DeleteWhenStopped);
        return false;
    }
};

// Pastille dorée qui glisse derrière le bouton de menu actif.
class NavPill : public QFrame {
public:
    NavPill(QWidget *sidebar, const QList<QPushButton *> &buttons) : QFrame(sidebar), m_buttons(buttons), m_move(this, "geometry")
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setStyleSheet("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #bfa370,stop:0.5 #e9d8a8,stop:1 #c4a876);border-radius:9px;");
        m_move.setDuration(Motion::Normal);
        m_move.setEasingCurve(QEasingCurve::OutCubic);
        for (QPushButton *b : buttons) {
            b->setStyleSheet("QPushButton:checked{background:transparent;}");
            b->raise();
            b->installEventFilter(this);
            connect(b, &QPushButton::toggled, this, [this, b](bool on) { if (on) follow(b, isVisible()); });
        }
        show();
        stackUnder(buttons.first());
    }

protected:
    bool eventFilter(QObject *o, QEvent *e) override
    {
        auto *b = static_cast<QPushButton *>(o);
        if ((e->type() == QEvent::Move || e->type() == QEvent::Resize || e->type() == QEvent::Show) && b->isChecked())
            follow(b, false);
        return false;
    }

private:
    void follow(QPushButton *b, bool animated)
    {
        m_move.stop();
        if (!animated) {
            setGeometry(b->geometry());
            return;
        }
        m_move.setStartValue(geometry());
        m_move.setEndValue(b->geometry());
        m_move.start();
    }
    QList<QPushButton *> m_buttons;
    QPropertyAnimation m_move;
};
}
