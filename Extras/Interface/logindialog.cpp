#include "logindialog.h"
#include "badgeservice.h"
#include "errorbanner.h"
#include "BadgeAnimation.h"
#include "ConnectionAnimation.h"
#include "badgelink.h"
#include "icons.h"
#include "sound.h"
#include <QComboBox>
#include <QStackedWidget>
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
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

LoginDialog::Role LoginDialog::authenticate(const QString &user, const QString &password)
{
    if (password != "123456") {
        return None;
    }
    const QString name = user.trimmed().toLower();
    if (name == "admin") {
        return Admin;
    }
    if (name == "employe" || name == QStringLiteral("employé")) {
        return EmployeeRole;
    }
    return None;
}

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("AUREA — Connexion"));
    setFixedSize(900, 560);
    QFile theme(":/theme.qss");
    if (theme.open(QIODevice::ReadOnly)) {
        setStyleSheet(QString::fromUtf8(theme.readAll()) + R"(
        LoginDialog { background:#1b1b1d; }
        QLabel#loginDate { color:#c9b27c; font-size:9pt; font-weight:600; }
        QLabel#welcome { font-size:21pt; font-weight:700; color:white; }
        QLineEdit[wrong="true"] { border:1px solid #b56a6a; }
        QLineEdit { min-height:26px; font-size:11pt; }
        QPushButton[primary="true"] { min-height:26px; font-size:11pt; })");
    }

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
    m_error = new ErrorBanner;
    m_error->unlocked = [this] {
        m_button->setEnabled(true);
        markFields(false);
    };
    for (QLineEdit *field : {m_user, m_password}) {
        connect(field, &QLineEdit::textEdited, this, [this] {
            m_error->clear();
            if (!m_error->locked()) {
                markFields(false);
            }
        });
    }
    m_button = new QPushButton(QStringLiteral("Se connecter"));
    m_button->setProperty("primary", true);
    m_button->setDefault(true);
    m_button->setCursor(Qt::PointingHandCursor);
    auto *date = new QLabel;
    date->setObjectName("loginDate");
    const auto showDate = [date] {
        date->setText(QLocale().toString(QDateTime::currentDateTime(), "dddd d MMMM · HH:mm:ss"));
    };
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
    auto *userLabel = new QLabel(QStringLiteral("Identifiant")),
         *passwordLabel = new QLabel(QStringLiteral("Mot de passe"));
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
    auto *badge = new QPushButton(QStringLiteral("  Se connecter avec un badge"));
    badge->setIcon(Icons::nav(Icons::Badge));
    badge->setIconSize(QSize(18, 18));
    badge->setCursor(Qt::PointingHandCursor);
    badge->setAutoDefault(false);
    badge->setMinimumHeight(42);
    form->insertWidget(form->indexOf(m_button) + 1, badge);
    connect(badge, &QPushButton::clicked, this, [this] {
        showBadge(true);
    });
    auto *account = new QWidget;
    account->setLayout(form);
    m_pages = new QStackedWidget;
    m_pages->addWidget(account);
    m_pages->addWidget(createBadgePanel());
    root->addWidget(m_pages, 1);
    connect(&BadgeLink::instance(), &BadgeLink::stateChanged, this, &LoginDialog::showLink);
    showLink();
    connect(m_button, &QPushButton::clicked, this, &LoginDialog::submit);
    Fx::steady(this);
    Fx::glowButtons(this);
    m_lines = {date, welcome, hint, userLabel, m_user, passwordLabel, m_password, m_button, badge, note};
    m_form = form;
}

QString LoginDialog::user() const
{
    return m_user->text().trimmed().toLower();
}

void LoginDialog::submit()
{
    if (m_leaving || m_role != None || m_error->locked()) {
        return;
    }
    m_role = authenticate(m_user->text(), m_password->text());
    if (m_role != None) {
        Sound::play(QStringLiteral("connexion"));
        for (QWidget *w : QList<QWidget *>{m_user, m_password, m_button}) {
            w->setEnabled(false);
        }
        m_error->clear();
        markFields(false);
        auto *dots = new QTimer(this);
        connect(dots, &QTimer::timeout, this, [this, n = 0]() mutable {
            m_button->setText(QStringLiteral("Connexion") + QString(++n % 4, '.'));
        });
        dots->start(120);
        leave(Motion::Quick);
        return;
    }
    Sound::play(QStringLiteral("erreur"));
    Fx::shake(this);
    if (refusedHandler) {
        refusedHandler(user().left(24));
    }
    m_password->clear();
    m_password->setFocus();
    markFields(true);
    if (++m_failures % 5 == 0) {
        m_button->setEnabled(false);
        m_error->lock(30);
    } else {
        m_error->showError(QStringLiteral("Identifiant ou mot de passe incorrect"),
            QStringLiteral("Vérifiez vos informations puis réessayez."));
    }
}

void LoginDialog::markFields(bool wrong)
{
    for (QLineEdit *field : {m_user, m_password}) {
        if (field->property("wrong").toBool() == wrong) {
            continue;
        }
        field->setProperty("wrong", wrong);
        field->style()->unpolish(field);
        field->style()->polish(field);
    }
}

LoginDialog::~LoginDialog() = default;

void LoginDialog::leave(int delay)
{
    QTimer::singleShot(delay, this, [this] {
        if (m_leaving) {
            return;
        }
        m_leaving = true;
        m_overlay = new MorphOverlay(this, m_scene->logoRect(), MorphOverlay::areaFor(this),
            m_role == Admin ? Spectacle::AdminIn : Spectacle::EmployeeIn, [this](bool on) {
                m_scene->setLogoVisible(on);
            });
        QTimer::singleShot(90, this, &QDialog::accept);
    });
}

QWidget *LoginDialog::createBadgePanel()
{
    auto *panel = new QWidget;
    auto *box = new QVBoxLayout(panel);
    box->setContentsMargins(62, 40, 62, 40);
    box->setSpacing(8);
    box->addStretch();
    m_badgeTitle = new QLabel;
    m_badgeTitle->setObjectName("welcome");
    m_badgeTitle->setAlignment(Qt::AlignCenter);
    m_badgeText = new QLabel;
    m_badgeText->setProperty("muted", true);
    m_badgeText->setAlignment(Qt::AlignCenter);
    m_badge = new BadgeAnimation;
    m_light = new LinkLight;
    m_linkText = new QLabel;
    m_linkText->setProperty("muted", true);
    auto *link = new QHBoxLayout;
    link->addStretch();
    link->addWidget(m_light);
    link->addWidget(m_linkText);
    link->addStretch();
    m_badges = new QComboBox;
    m_scanButton = new QPushButton(QStringLiteral("Passer"));
    m_scanButton->setProperty("primary", true);
    m_scanButton->setAutoDefault(false);
    auto *row = new QHBoxLayout;
    row->addWidget(m_badges, 1);
    row->addWidget(m_scanButton);
    auto *back = new QPushButton(QStringLiteral("Utiliser un identifiant"));
    back->setCursor(Qt::PointingHandCursor);
    back->setAutoDefault(false);
    box->addWidget(m_badgeTitle);
    box->addWidget(m_badgeText);
    box->addWidget(m_badge);
    box->addLayout(link);
    box->addSpacing(10);
    box->addLayout(row);
    box->addWidget(back);
    box->addStretch();
    connect(back, &QPushButton::clicked, this, [this] {
        if (m_role == None) {
            showBadge(false);
        }
    });
    connect(m_scanButton, &QPushButton::clicked, this, [this] {
        scanBadge(QStringLiteral("BADGE:") + m_badges->currentData().toString());
    });
    return panel;
}

void LoginDialog::showLink()
{
    const BadgeLink &link = BadgeLink::instance();
    m_light->setState(LinkLight::State(link.state()));
    m_linkText->setText(
        link.state() == BadgeLink::On ? QStringLiteral("Lecteur prêt") : QStringLiteral("Badge virtuel disponible"));
}

void LoginDialog::showBadge(bool on)
{
    if (on) {
        m_badgeTitle->setText(QStringLiteral("Badge"));
        m_badgeText->setText(QStringLiteral("Présentez votre badge au lecteur."));
        if (!m_listed && badgeList) {
            m_badges->clear();
            for (const auto &b : badgeList()) {
                m_badges->addItem(b.second + QStringLiteral(" · ") + BadgeService::cardName(b.first), b.first);
            }
            m_badges->addItem(QStringLiteral("Badge inconnu · 0BADBAD0"), QStringLiteral("0BADBAD0"));
            m_listed = true;
        }
    }
    Fx::switchPage(m_pages, on ? 1 : 0);
}

QString LoginDialog::scanBadge(const QString &line, bool board)
{
    if (m_leaving || m_role != None || !badgeHandler || m_badge->mode() == BadgeAnimation::Checking) {
        return QStringLiteral("WAIT;AUREA");
    }
    if (m_pages->currentIndex() != 1) {
        showBadge(true);
    }
    m_badge->present();
    m_badgeText->setText(QStringLiteral("Vérification…"));
    m_scanButton->setEnabled(false);
    if (!board) {
        QTimer::singleShot(Motion::Base, this, [this, line] {
            if (!m_leaving && m_role == None) {
                answerBadge(badgeHandler(line.section(':', 1)));
            }
        });
        return QString();
    }
    const BadgeResult result = badgeHandler(line.section(':', 1));
    answerBadge(result);
    return result.reply;
}

void LoginDialog::answerBadge(const BadgeResult &result)
{
    if (result.role != None) {
        m_role = result.role;
        m_employeeId = result.employeeId;
        m_usedBadge = true;
    }
    const bool ok = result.role != None;
    m_badge->answer(ok);
    Sound::play(ok ? QStringLiteral("connexion") : QStringLiteral("erreur"));
    m_badgeTitle->setText(ok ? QStringLiteral("Accès autorisé") : QStringLiteral("Accès refusé"));
    m_badgeText->setText(ok         ? result.name
            : result.reply == "ERR" ? QStringLiteral("Base de données indisponible.")
                                    : QStringLiteral("Ce badge n'est pas reconnu."));
    Fx::reveal(m_badgeTitle, 0, Motion::Base);
    Fx::reveal(m_badgeText, 0, Motion::Base);
    if (ok) {
        leave(Motion::Slow);
        return;
    }
    QTimer::singleShot(2200, this, [this] {
        m_scanButton->setEnabled(true);
        if (m_role != None) {
            return;
        }
        m_badgeTitle->setText(QStringLiteral("Badge"));
        m_badgeText->setText(QStringLiteral("Présentez votre badge au lecteur."));
    });
}

MorphOverlay *LoginDialog::takeOverlay()
{
    MorphOverlay *overlay = m_overlay;
    m_overlay = nullptr;
    return overlay;
}

void LoginDialog::listen()
{
    BadgeLink::instance().receiver = [this](const QString &line) {
        return scanBadge(line, true);
    };
}

void LoginDialog::arriveFrom(MorphOverlay *overlay)
{
    listen();
    m_scene->skipIntro();
    m_scene->setLogoVisible(false);
    QTimer::singleShot(0, this, [this, overlay] {
        overlay->land(
            [this] {
                return m_scene->logoRect();
            },
            [this](bool on) {
                m_scene->setLogoVisible(on);
            });
    });
}

void LoginDialog::reject()
{
    if (m_leaving) {
        return;
    }
    m_leaving = true;
    m_role = None;
    m_overlay =
        new MorphOverlay(this, m_scene->logoRect(), MorphOverlay::areaFor(this), Spectacle::Closing, [this](bool on) {
            setProperty("ringTime", m_scene->time());
            m_scene->setRingsVisible(on);
            m_scene->setLogoVisible(on);
        });
    QTimer::singleShot(90, this, [this] {
        QDialog::reject();
    });
}

void LoginDialog::start()
{
    listen();
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
    connect(slide, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_form->setContentsMargins(v.toInt(), 40, 124 - v.toInt(), 40);
    });
    QTimer::singleShot(100, slide, [slide] {
        slide->start(QAbstractAnimation::DeleteWhenStopped);
    });
}
