#include "dialogskin.h"
#include "AnimationSpeed.h"
#include "LogoAnimation.h"
#include <QAbstractButton>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <cmath>

namespace {
class Backdrop : public QWidget {
public:
    explicit Backdrop(QWidget *window)
        : QWidget(window)
    {
        setGeometry(window->rect());
        show();
        raise();
        go(0, 1, Motion::Base);
    }
    QVariantAnimation *go(qreal from, qreal to, int duration)
    {
        auto *a = new QVariantAnimation(this);
        a->setDuration(duration);
        a->setStartValue(from);
        a->setEndValue(to);
        a->setEasingCurve(QEasingCurve::OutCubic);
        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            m_level = v.toReal();
            update();
        });
        a->start(QAbstractAnimation::DeleteWhenStopped);
        return a;
    }
    void leave()
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        connect(go(m_level, 0, Motion::Base), &QAbstractAnimation::finished, this, &QObject::deleteLater);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter(this).fillRect(rect(), QColor(10, 10, 11, int(165 * m_level)));
    }
    void mousePressEvent(QMouseEvent *) override { }

private:
    qreal m_level = 0;
};

class Ghost : public QWidget {
public:
    Ghost(QWidget *window, const QPixmap &shot, const QRect &area)
        : QWidget(window)
        , m_shot(shot)
        , m_area(area)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setGeometry(area.adjusted(-60, -60, 60, 60));
        show();
        raise();
    }
    QVariantAnimation *play(bool opening)
    {
        auto *a = new QVariantAnimation(this);
        a->setDuration(opening ? Motion::Base : Motion::Quick);
        a->setStartValue(0.0);
        a->setEndValue(1.0);
        connect(a, &QVariantAnimation::valueChanged, this, [this, opening](const QVariant &v) {
            const double t = v.toReal();
            const double out = 1 - std::pow(1 - t, 3);
            m_scale = opening ? 0.96 + 0.04 * out : 1 - 0.03 * out;
            m_alpha = opening ? std::min(1.0, t * 2.2) : 1 - out;
            m_shift = opening ? 10 * (1 - out) : 6 * out;
            update();
        });
        connect(a, &QAbstractAnimation::finished, this, &QObject::deleteLater);
        a->start();
        return a;
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.setOpacity(m_alpha);
        p.translate(width() / 2.0, height() / 2.0 + m_shift);
        p.scale(m_scale, m_scale);
        p.drawPixmap(QPointF(-m_area.width() / 2.0, -m_area.height() / 2.0), m_shot);
    }

private:
    QPixmap m_shot;
    QRect m_area;
    double m_scale = 0.96, m_alpha = 0, m_shift = 10;
};

int message(QWidget *parent, LivingIcon::Kind kind, const QString &title, const QString &text, const QString &yes,
    const QString &no)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    auto *box = new QVBoxLayout(&dialog);
    box->setSpacing(22);
    auto *row = new QHBoxLayout;
    row->setSpacing(22);
    row->addWidget(new LivingIcon(kind, 78), 0, Qt::AlignTop);
    auto *texts = new QVBoxLayout;
    texts->setSpacing(7);
    auto *head = new QLabel(title);
    head->setProperty("section", true);
    auto *body = new QLabel(text);
    body->setWordWrap(true);
    body->setMinimumWidth(text.size() > 160 ? 440 : 270);
    body->setStyleSheet("color:#cdbd92;");
    for (QLabel *l : {head, body}) {
        l->setTextFormat(Qt::PlainText);
        texts->addWidget(l);
    }
    texts->addStretch();
    row->addLayout(texts, 1);
    box->addLayout(row);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    if (!no.isEmpty()) {
        auto *b = new QPushButton(no);
        b->setAutoDefault(false);
        QObject::connect(b, &QPushButton::clicked, &dialog, &QDialog::reject);
        buttons->addWidget(b);
    }
    auto *b = new QPushButton(yes);
    b->setProperty("primary", true);
    b->setDefault(true);
    QObject::connect(b, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttons->addWidget(b);
    box->addLayout(buttons);
    return DialogSkin::run(dialog);
}
}

bool DialogSkin::ask(QWidget *parent, const QString &title, const QString &text, const QString &yes, const QString &no)
{
    return message(parent, LivingIcon::Question, title, text, yes, no) == QDialog::Accepted;
}

void DialogSkin::info(QWidget *parent, const QString &title, const QString &text)
{
    message(parent, LivingIcon::Info, title, text, QStringLiteral("Compris"), QString());
}
void DialogSkin::warn(QWidget *parent, const QString &title, const QString &text)
{
    message(parent, LivingIcon::Alert, title, text, QStringLiteral("Compris"), QString());
}

int DialogSkin::run(QDialog &dialog)
{
    QWidget *owner = dialog.parentWidget() ? dialog.parentWidget()->window() : nullptr;
    if (owner && !owner->isVisible()) {
        owner = nullptr;
    }
    DialogSkin skin(&dialog, owner);
    return dialog.exec();
}

DialogSkin::DialogSkin(QDialog *dialog, QWidget *owner)
    : m_dialog(dialog)
    , m_owner(owner)
{
    dialog->setObjectName("aureaDialog");
    if (auto *form = qobject_cast<QFormLayout *>(dialog->layout())) {
        auto *title = new QLabel(dialog->windowTitle());
        title->setProperty("section", true);
        form->insertRow(0, title);
        form->setVerticalSpacing(13);
    }
    if (auto *buttons = dialog->findChild<QDialogButtonBox *>()) {
        for (QAbstractButton *b : buttons->buttons()) {
            b->setProperty("primary", buttons->buttonRole(b) == QDialogButtonBox::AcceptRole);
            b->setIcon(QIcon());
        }
    }
    for (QAbstractButton *b : dialog->findChildren<QAbstractButton *>()) {
        b->setCursor(Qt::PointingHandCursor);
    }
    if (owner) {
        dialog->setParent(owner, Qt::Widget);
    } else {
        dialog->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    }
    dialog->setStyleSheet(QStringLiteral(
        "QDialog#aureaDialog{background:%1;border:none;}"
        "QDialog#aureaDialog QLabel{background:transparent;color:#f3f0ea;font-family:'Inter';font-size:10.5pt;}"
        "QDialog#aureaDialog QLabel[section=\"true\"]{font-size:15pt;font-weight:700;color:#ffffff;}"
        "QDialog#aureaDialog QLabel[muted=\"true\"]{font-size:9.5pt;color:#a9a294;}"
        "QDialog#aureaDialog QPlainTextEdit{background:#17171a;border:1px solid #4a4434;border-radius:10px;padding:8px;color:#f3f0ea;}"
        "QDialog#aureaDialog QPushButton{background:#2a2a2d;color:#eeeae1;border:1px solid #4a4434;border-radius:10px;padding:10px 22px;min-width:78px;font-weight:600;font-size:10pt;}"
        "QDialog#aureaDialog QPushButton:hover{border-color:#e2cf9f;background:#36342e;}"
        "QDialog#aureaDialog QPushButton[primary=\"true\"]{color:#1c1c1e;border:none;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #c4a876,stop:0.5 #ebdcae,stop:1 #c9ad7a);}"
        "QDialog#aureaDialog QPushButton[primary=\"true\"]:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #d0b684,stop:0.5 #f4e7bc,stop:1 #d4b988);}")
            .arg(owner ? "transparent" : "#1f1e1a"));
    if (dialog->layout()) {
        dialog->layout()->setContentsMargins(34, 30, 34, 28);
    }
    dialog->installEventFilter(this);
    if (owner) {
        owner->installEventFilter(this);
    }
    if (owner) {
        for (QShortcut *shortcut : owner->findChildren<QShortcut *>()) {
            if (shortcut->isEnabled()) {
                shortcut->setEnabled(false);
                m_shortcuts.append(shortcut);
            }
        }
    }
}

DialogSkin::~DialogSkin()
{
    for (const QPointer<QShortcut> &shortcut : std::as_const(m_shortcuts)) {
        if (shortcut) {
            shortcut->setEnabled(true);
        }
    }
}

void DialogSkin::place()
{
    if (!m_owner) {
        return;
    }
    m_dialog->adjustSize();
    m_dialog->move(m_owner->rect().center() - QPoint(m_dialog->width() / 2, m_dialog->height() / 2));
}

void DialogSkin::appear()
{
    if (!m_owner) {
        return;
    }
    m_backdrop = new Backdrop(m_owner);
    place();
    m_dialog->raise();
    const QPixmap shot = m_dialog->grab();
    auto *effect = new QGraphicsOpacityEffect(m_dialog);
    effect->setOpacity(0);
    m_dialog->setGraphicsEffect(effect);
    auto *ghost = new Ghost(m_owner, shot, m_dialog->geometry());
    QDialog *dialog = m_dialog;
    connect(ghost->play(true), &QAbstractAnimation::finished, dialog, [dialog] {
        dialog->setGraphicsEffect(nullptr);
    });
}

void DialogSkin::disappear()
{
    if (!m_owner) {
        return;
    }
    if (m_backdrop) {
        static_cast<Backdrop *>(m_backdrop.data())->leave();
    }
    m_dialog->setGraphicsEffect(nullptr);
    auto *ghost = new Ghost(m_owner, m_dialog->grab(), m_dialog->geometry());
    ghost->play(false);
}

bool DialogSkin::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_owner) {
        if (event->type() == QEvent::Close && m_dialog->isVisible()) {
            event->ignore();
            return true;
        }
        if (event->type() == QEvent::Resize) {
            place();
            if (m_backdrop) {
                m_backdrop->setGeometry(m_owner->rect());
            }
        }
        return false;
    }
    if (object != m_dialog) {
        return false;
    }
    if (event->type() == QEvent::Show) {
        appear();
    } else if (event->type() == QEvent::Hide) {
        disappear();
    } else if (event->type() == QEvent::Paint && m_owner) {
        QPainter p(m_dialog);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(m_dialog->rect()).adjusted(1.5, 1.5, -1.5, -1.5);
        QLinearGradient back(r.topLeft(), r.bottomRight());
        back.setColorAt(0, QColor("#1c1c1f"));
        back.setColorAt(0.6, QColor("#22201b"));
        back.setColorAt(1, QColor("#33291a"));
        p.setBrush(back);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(r, 20, 20);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#6b5f43"), 1.2));
        p.drawRoundedRect(r, 20, 20);
    }
    return false;
}
