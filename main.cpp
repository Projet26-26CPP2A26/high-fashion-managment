#include "activityboard.h"
#include "activitystore.h"
#include "dialogskin.h"
#include "employeedesk.h"
#include "employeemodule.h"
#include "employeerepository.h"
#include "employeewindow.h"
#include "fx.h"
#include "greeting.h"
#include "logindialog.h"
#include "morphoverlay.h"
#include "oraclecheck.h"
#include "oracleconnection.h"
#include "oraclemodulecheck.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QEventLoop>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QScreen>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <cstdio>
#include <memory>

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    if (qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR"))
        qputenv("QT_QPA_FONTDIR", QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts").toUtf8());
#endif
    QApplication app(argc, argv);
    app.setApplicationName("AUREA");
    app.setOrganizationName("Velora");
    app.setApplicationDisplayName("AUREA — Gestion des employés");
    app.setWindowIcon(QIcon(":/assets/logo_aurea.png"));
    app.setStyle("Fusion");
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));

    for (const QString &file : QStringList{"segoeui.ttf", "segoeuib.ttf", "georgia.ttf"})
        QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts/" + file));
    for (const QString &file : QStringList{"Inter.ttf", "CormorantGaramond.ttf", "CormorantGaramond-Italic.ttf"})
        QFontDatabase::addApplicationFont(":/assets/fonts/" + file);
    app.setFont(QFont("Inter", 10));
    app.setStyleSheet("QToolTip { background: #eee2bf; color: #23292b; border: none; padding: 7px; }");

    QCommandLineParser parser;
    parser.setApplicationDescription("AUREA — Gestion des employés (C++ / Qt Widgets)");
    parser.addHelpOption();
    parser.addOption({"oracle-test", "Tester la connexion Oracle (ODBC, DSN Source_Projet2A) puis quitter."});
    parser.addOption({"oracle-test-modules", "Tester le module Employés sur Oracle (CRUD, données d'essai supprimées) puis quitter."});
    parser.addOption({"oracle", "Conservé pour compatibilité : Oracle est toujours utilisé."});
    parser.addOption({"live", "Utilisation réelle : sans les employés animés de la démonstration."});
    parser.process(app);

    if (parser.isSet("oracle-test") || parser.isSet("oracle-test-modules")) {
        const OracleCheck::Report report = parser.isSet("oracle-test-modules") ? OracleCheck::runModules() : OracleCheck::run();
        std::fprintf(stdout, "%s\n", qPrintable(report.text));
        std::fflush(stdout);
        QMessageBox box(report.ok ? QMessageBox::Information : QMessageBox::Warning, "AUREA — test Oracle",
                        report.text, QMessageBox::Ok);
        box.setTextFormat(Qt::PlainText);
        box.exec();
        return report.ok ? 0 : 2;
    }

    struct OracleShutdown { ~OracleShutdown() { OracleConnection::instance().shutdown(); } } oracleShutdown;
    app.setQuitOnLastWindowClosed(false);

    const bool demo = !parser.isSet("live");
    ActivityStore store;
    if (!store.open(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + (demo ? "/activity_demo.sqlite" : "/activity.sqlite"))) {
        DialogSkin::warn(nullptr, "AUREA", QStringLiteral("Suivi d'activité indisponible : ") + store.error());
        return 1;
    }
    MorphOverlay *overlay = nullptr;
    const auto logoOf = [](QWidget *window) {
        const QLabel *logo = window->findChild<QLabel *>("logo");
        if (!logo) return QRect(window->mapToGlobal(QPoint(80, 30)), QSize(64, 64));
        const QSize size = logo->pixmap().isNull() ? QSize(64, 64) : logo->pixmap().size() / logo->pixmap().devicePixelRatio();
        return QRect(logo->mapToGlobal(QPoint((logo->width() - size.width()) / 2, (logo->height() - size.height()) / 2)), size);
    };
    const auto logoSwitch = [](QWidget *window) {
        return MorphOverlay::LogoSwitch([window = QPointer<QWidget>(window)](bool on) {
            QLabel *logo = window ? window->findChild<QLabel *>("logo") : nullptr;
            if (!logo) return;
            if (!logo->property("image").isValid()) logo->setProperty("image", QVariant::fromValue(logo->pixmap()));
            logo->setProperty("logoHidden", !on);
            QPixmap image = logo->property("image").value<QPixmap>();
            if (!on) image.fill(Qt::transparent);
            logo->setPixmap(image);
            if (on) logo->repaint();
        });
    };
    const auto loginArea = [](QWidget *window) {
        const QRect area = window->screen()->availableGeometry();
        return QRect(area.center() - QPoint(450, 280), QSize(900, 560));
    };
    const QString adminName = QStringLiteral("ADMIN"), adminEvent = QStringLiteral("Connexion du responsable RH");

    MorphOverlay::warmUp();
    std::unique_ptr<QThread> warm(QThread::create([] { OracleConnection::warmUp(); }));
    warm->start();
    struct WarmEnd { QThread *thread; ~WarmEnd() { thread->wait(); } } warmEnd{warm.get()};
    std::unique_ptr<LoginDialog> next(new LoginDialog);
    std::unique_ptr<EmployeeDesk> desk;
    std::unique_ptr<EmployeeWindow> window;

    for (;;) {
        std::unique_ptr<LoginDialog> login = std::move(next);
        if (!login) login.reset(new LoginDialog);
        if (overlay) {
            overlay->waitExpanded();
            overlay->waitMessage();
            login->arriveFrom(overlay);
            overlay = nullptr;
        } else {
            login->start();
        }
        const bool accepted = login->exec() == QDialog::Accepted;
        overlay = login->takeOverlay();
        if (!accepted) {
            if (overlay) {
                overlay->setMessage(QStringLiteral("À bientôt"), QString());
                overlay->waitClosed();
            }
            return 0;
        }
        const LoginDialog::Role role = login->role();
        overlay->waitExpanded();
        const auto stop = [&](const QString &text) {
            overlay->close();
            overlay = nullptr;
            DialogSkin::warn(nullptr, QStringLiteral("AUREA"), text);
        };
        desk.reset();
        window.reset();
        MorphOverlay::breathe();

        QEventLoop loop;
        bool logout = false;
        QWidget *space = nullptr;
        const auto leave = [&](bool again, const QString &title, const QString &subtitle) {
            logout = again;
            if (!qApp->isSavingSession()) {
                overlay = new MorphOverlay(space, logoOf(space), again ? loginArea(space) : MorphOverlay::areaFor(space), !again ? Spectacle::Closing : desk ? Spectacle::EmployeeOut : Spectacle::AdminOut, logoSwitch(space));
                overlay->setMessage(title, subtitle);
            }
            loop.quit();
        };
        OracleConnection &oracle = OracleConnection::instance();
        if (!oracle.open()) {
            stop(oracle.errorMessage());
            continue;
        }
        MorphOverlay::breathe();
        const QDate today = QDate::currentDate();
        if (role == LoginDialog::EmployeeRole) {
            EmployeeRepository people;
            Employee employee;
            const QVector<Employee> list = people.loadEmployees();
            for (const Employee &e : list)
                if (!employee.id() || e.id() < employee.id()) employee = e;
            if (!people.error().isEmpty() || !employee.id()) {
                stop(people.error().isEmpty() ? QStringLiteral("Aucun employé dans la base.") : people.error());
                continue;
            }
            if (store.hasLiveSession(employee.reference())) {
                stop(QStringLiteral("Cette session est déjà ouverte (ou vient d'être interrompue).\nRéessayez dans 20 secondes."));
                continue;
            }
            const int ever = store.countSessions(employee.reference(), QDate()), before = store.countSessions(employee.reference(), today);
            desk.reset(new EmployeeDesk(store, employee));
            MorphOverlay::breathe();
            if (!desk->ready()) {
                desk.reset();
                stop(store.error());
                continue;
            }
            const QString firstName = employee.name().section(' ', 0, 0);
            space = desk.get();
            QObject::connect(desk.get(), &EmployeeDesk::closed, &loop, [&, firstName](bool again) {
                leave(again, QStringLiteral("À bientôt, ") + firstName, QStringLiteral("Travail enregistré : ") + clockText(desk->workToday()));
            });
            Fx::steady(desk.get());
            desk->showMaximized();
            MorphOverlay::breathe();
            overlay->setMessage(Greeting::hello(ever, before, firstName), Greeting::tasks(store.tasks(employee.reference())));
        } else {
            const int ever = store.countEvents(adminName, adminEvent, QDate()), before = store.countEvents(adminName, adminEvent, today);
            window.reset(new EmployeeWindow(store, demo));
            MorphOverlay::breathe();
            if (!window->ready()) {
                window.reset();
                stop(QString("Impossible de lire la table EMPLOYEES avec l'utilisateur « %1 ».\n"
                                                    "Vérifiez que oracle/01_schema.sql a été exécuté avec cet utilisateur.")
                                                .arg(OracleConnection::settings().user));
                continue;
            }
            store.logAt(adminName, adminEvent, QDateTime::currentDateTime());
            space = window.get();
            QObject::connect(window.get(), &EmployeeWindow::closed, &loop, [&](bool again) {
                leave(again, QStringLiteral("À bientôt"), QStringLiteral("Vos données sont enregistrées."));
            });
            Fx::steady(window.get());
            window->showMaximized();
            MorphOverlay::breathe();
            auto *board = window->findChild<ActivityBoard *>();
            overlay->setMessage(Greeting::hello(ever, before, QStringLiteral("responsable RH")), Greeting::team(board->connected(), int(board->reviews().size())));
        }
        overlay->land([&logoOf, space] { return logoOf(space); }, logoSwitch(space));
        overlay = nullptr;
        login.reset();
        QTimer::singleShot(4200, &loop, [&] { if (!next) next.reset(new LoginDialog); });
        loop.exec();
        if (logout) continue;
        if (overlay) {
            overlay->waitClosed();
        }
        return 0;
    }
}
