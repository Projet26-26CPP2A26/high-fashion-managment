#include "logindialog.h"
#include "UiAnimations.h"
#include "LoginAnimation.h"
#include "TransitionAnimation.h"
#include <QFile>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

LoginDialog::Role LoginDialog::authenticate(const QString &user, const QString &password)
{
    if (password != "123456") return None;
    const QString name = user.trimmed().toLower();
    if (name == "admin") return Admin;
    if (name == "employe" || name == QStringLiteral("employé")) return EmployeeRole;
    return None;
}

LoginDialog::LoginDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("AUREA — Connexion"));
    setFixedSize(900, 560);
    QFile theme(":/theme.qss");
    if (theme.open(QIODevice::ReadOnly)) setStyleSheet(QString::fromUtf8(theme.readAll()) + R"(
        LoginDialog { background:#1b1b1d; }
        QLabel#loginDate { color:#c9b27c; font-size:9pt; font-weight:600; }
        QLabel#welcome { font-size:21pt; font-weight:700; color:white; }
        QLabel#loginError { color:#f0b9bb; font-size:9pt; }
        QLineEdit { min-height:26px; font-size:11pt; }
        QPushButton[primary="true"] { min-height:26px; font-size:11pt; })");

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    m_scene = new LoginScene;
    m_scene->setFixedWidth(400);
    root->addWidget(m_scene);

    m_user = new QLineEdit;
    m_user->setPlaceholderText(QStringLiteral("admin ou employe"));
    m_password = new QLineEdit;
    m_password->setPlaceholderText(QStringLiteral("••••••"));
    m_password->setEchoMode(QLineEdit::Password);
    m_error = new QLabel;
    m_error->setObjectName("loginError");
    m_error->setMinimumHeight(20);
    m_button = new QPushButton(QStringLiteral("Se connecter"));
    m_button->setProperty("primary", true);
    m_button->setDefault(true);
    m_button->setCursor(Qt::PointingHandCursor);
    auto *date = new QLabel;
    date->setObjectName("loginDate");
    const auto showDate = [date] { date->setText(QLocale().toString(QDateTime::currentDateTime(), "dddd d MMMM · HH:mm:ss")); };
    showDate();
    auto *clock = new QTimer(this);
    connect(clock, &QTimer::timeout, this, showDate);
    clock->start(1000);
    const int hour = QTime::currentTime().hour();
    auto *welcome = new QLabel(hour >= 18 || hour < 5 ? QStringLiteral("Bonsoir") : QStringLiteral("Bonjour"));
    welcome->setObjectName("welcome");
    auto *hint = new QLabel(QStringLiteral("Connectez-vous à votre espace."));
    hint->setProperty("muted", true);
    auto *note = new QLabel(QStringLiteral("Espace employé : la connexion démarre le chronomètre de travail."));
    note->setProperty("muted", true);
    note->setWordWrap(true);

    auto *form = new QVBoxLayout;
    form->setContentsMargins(62, 40, 62, 40);
    form->setSpacing(10);
    form->addStretch();
    form->addWidget(date);
    form->addWidget(welcome);
    form->addWidget(hint);
    form->addSpacing(18);
    auto *userLabel = new QLabel(QStringLiteral("Identifiant")), *passwordLabel = new QLabel(QStringLiteral("Mot de passe"));
    form->addWidget(userLabel);
    form->addWidget(m_user);
    form->addSpacing(6);
    form->addWidget(passwordLabel);
    form->addWidget(m_password);
    form->addWidget(m_error);
    form->addWidget(m_button);
    form->addSpacing(8);
    form->addWidget(note);
    form->addStretch();
    root->addLayout(form, 1);
    connect(m_button, &QPushButton::clicked, this, &LoginDialog::submit);
    Fx::steady(this);
    Fx::glowButtons(this);
    m_lines = {date, welcome, hint, userLabel, m_user, passwordLabel, m_password, m_button, note};
    m_form = form;
}

QString LoginDialog::user() const { return m_user->text().trimmed().toLower(); }

void LoginDialog::submit()
{
    if (m_leaving || m_role != None) return;
    m_role = authenticate(m_user->text(), m_password->text());
    if (m_role != None) {
        for (QWidget *w : QList<QWidget *>{m_user, m_password, m_button}) w->setEnabled(false);
        m_error->clear();
        auto *dots = new QTimer(this);
        connect(dots, &QTimer::timeout, this, [this, n = 0]() mutable { m_button->setText(QStringLiteral("Connexion") + QString(++n % 4, '.')); });
        dots->start(120);
        QTimer::singleShot(260, this, [this] {
            if (m_leaving) return;
            m_leaving = true;
            m_overlay = new MorphOverlay(this, m_scene->logoRect(), MorphOverlay::areaFor(this), m_role == Admin ? Spectacle::AdminIn : Spectacle::EmployeeIn, [this](bool on) { m_scene->setLogoVisible(on); });
            QTimer::singleShot(90, this, &QDialog::accept);
        });
        return;
    }
    Fx::shake(this);
    m_password->clear();
    m_password->setFocus();
    m_error->setText(QStringLiteral("Identifiant ou mot de passe incorrect."));
    if (++m_failures % 5 == 0) {
        m_button->setEnabled(false);
        m_error->setText(QStringLiteral("Trop d'essais : réessayez dans 30 secondes."));
        QTimer::singleShot(30000, this, [this] { m_button->setEnabled(true); m_error->clear(); });
    }
}

MorphOverlay *LoginDialog::takeOverlay()
{
    MorphOverlay *overlay = m_overlay;
    m_overlay = nullptr;
    return overlay;
}

void LoginDialog::arriveFrom(MorphOverlay *overlay)
{
    m_scene->skipIntro();
    m_scene->setLogoVisible(false);
    QTimer::singleShot(0, this, [this, overlay] { overlay->land([this] { return m_scene->logoRect(); }, [this](bool on) { m_scene->setLogoVisible(on); }); });
}

void LoginDialog::reject()
{
    if (m_leaving) return;
    m_leaving = true;
    m_role = None;
    m_overlay = new MorphOverlay(this, m_scene->logoRect(), MorphOverlay::areaFor(this), Spectacle::Closing, [this](bool on) {
        setProperty("ringTime", m_scene->time());
        m_scene->setRingsVisible(on);
        m_scene->setLogoVisible(on);
    });
    QTimer::singleShot(90, this, [this] { QDialog::reject(); });
}

void LoginDialog::start()
{
    Fx::openWindow(this);
    int delay = 120;
    for (QWidget *w : std::as_const(m_lines)) {
        Fx::reveal(w, delay);
        delay += Motion::Step;
    }
    auto *slide = new QVariantAnimation(this);
    slide->setDuration(Motion::Slow);
    slide->setStartValue(76);
    slide->setEndValue(62);
    slide->setEasingCurve(QEasingCurve::OutCubic);
    connect(slide, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) { m_form->setContentsMargins(v.toInt(), 40, 124 - v.toInt(), 40); });
    QTimer::singleShot(100, slide, [slide] { slide->start(QAbstractAnimation::DeleteWhenStopped); });
}
