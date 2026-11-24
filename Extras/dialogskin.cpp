#include "dialogskin.h"
#include "AnimationSpeed.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QVBoxLayout>

DialogSkin::DialogSkin(QWidget *parent, LivingIcon::Kind kind, const QString &title, const QString &text, const QString &accept, const QString &reject)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
{
    setWindowTitle(title);
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("QDialog{background:transparent;}");
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(38, 34, 38, 30);
    root->setSpacing(24);
    root->addWidget(new LivingIcon(kind), 0, Qt::AlignTop);
    auto *column = new QVBoxLayout;
    column->setSpacing(12);
    auto *head = new QLabel(title);
    head->setStyleSheet("font-family:'Cormorant Garamond','Georgia';font-size:20pt;color:#e9d8a8;background:transparent;");
    auto *body = new QLabel(text);
    body->setTextFormat(Qt::PlainText);
    body->setWordWrap(true);
    body->setMinimumWidth(text.size() > 300 ? 560 : 360);
    body->setStyleSheet("color:#d3cfc5;background:transparent;");
    column->addWidget(head);
    column->addWidget(body);
    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 10, 0, 0);
    buttons->addStretch();
    if (!reject.isEmpty()) {
        auto *no = new QPushButton(reject);
        no->setAutoDefault(false);
        connect(no, &QPushButton::clicked, this, &QDialog::reject);
        buttons->addWidget(no);
    }
    auto *yes = new QPushButton(accept);
    yes->setObjectName("accept");
    yes->setProperty("primary", true);
    yes->setDefault(reject.isEmpty());
    yes->setAutoDefault(false);
    connect(yes, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(yes);
    column->addLayout(buttons);
    root->addLayout(column, 1);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        update();
        if (m_clock.elapsed() > 1300)
            m_timer.stop();
    });
}

bool DialogSkin::ask(QWidget *parent, const QString &title, const QString &text, const QString &accept)
{
    DialogSkin dialog(parent, LivingIcon::Question, title, text, accept, QStringLiteral("Annuler"));
    return dialog.exec() == QDialog::Accepted;
}

void DialogSkin::warn(QWidget *parent, const QString &title, const QString &text)
{
    DialogSkin(parent, LivingIcon::Error, title, text, QStringLiteral("Compris")).exec();
}

void DialogSkin::info(QWidget *parent, const QString &title, const QString &text)
{
    DialogSkin(parent, LivingIcon::Info, title, text, QStringLiteral("Fermer")).exec();
}

void DialogSkin::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    m_clock.start();
    m_timer.start(Motion::frame(this));
    if (QWidget *owner = parentWidget()) {
        owner = owner->window();
        if (!m_shade) {
            m_shade = new QWidget(owner);
            m_shade->setObjectName("dialogShade");
            m_shade->setAttribute(Qt::WA_TransparentForMouseEvents);
            m_shade->setStyleSheet("#dialogShade{background:rgba(10,10,12,130);}");
            m_shade->setGeometry(owner->rect());
            m_shade->show();
            owner->installEventFilter(this);
        }
        move(owner->mapToGlobal(owner->rect().center()) - rect().center());
    }
    const QPoint home = pos();
    setWindowOpacity(0);
    auto *fade = new QPropertyAnimation(this, "windowOpacity", this);
    fade->setDuration(Motion::Normal);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->start(QAbstractAnimation::DeleteWhenStopped);
    auto *rise = new QPropertyAnimation(this, "pos", this);
    rise->setDuration(Motion::Normal);
    rise->setStartValue(home + QPoint(0, 22));
    rise->setEndValue(home);
    rise->setEasingCurve(QEasingCurve::OutCubic);
    rise->start(QAbstractAnimation::DeleteWhenStopped);
}

bool DialogSkin::eventFilter(QObject *object, QEvent *event)
{
    if (m_shade && event->type() == QEvent::Resize && object == m_shade->parent())
        m_shade->setGeometry(m_shade->parentWidget()->rect());
    return QDialog::eventFilter(object, event);
}

void DialogSkin::done(int result)
{
    if (m_shade)
        m_shade->deleteLater();
    m_shade = nullptr;
    QDialog::done(result);
}

void DialogSkin::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF card = QRectF(rect()).adjusted(8, 8, -8, -8);
    QLinearGradient back(card.topLeft(), card.bottomRight());
    back.setColorAt(0, QColor("#2a2823"));
    back.setColorAt(0.5, QColor("#212124"));
    back.setColorAt(1, QColor("#1b1b1d"));
    p.setPen(QPen(QColor("#3a3a3f"), 1));
    p.setBrush(back);
    p.drawRoundedRect(card, 18, 18);
    const double t = m_clock.isValid() ? Motion::both(m_clock.elapsed() / 1200.0) : 1;
    QPainterPath border;
    border.addRoundedRect(card, 18, 18);
    const double length = border.length() / 1.6;
    QPen pen(QColor("#c9b27c"), 1.6);
    pen.setDashPattern({std::max(0.01, length * t), length + 10});
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawPath(border);
    if (t < 1) {
        const QPointF head = border.pointAtPercent(t);
        QRadialGradient spark(head, 14);
        spark.setColorAt(0, QColor(255, 246, 220, 230));
        spark.setColorAt(1, QColor(255, 246, 220, 0));
        p.fillRect(QRectF(head - QPointF(14, 14), QSizeF(28, 28)), spark);
    }
}
