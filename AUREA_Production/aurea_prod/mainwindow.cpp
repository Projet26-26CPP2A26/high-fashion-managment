#include "mainwindow.h"
#include "pages.h"
#include "uikit.h"
#include <QtWidgets>

MainWindow::MainWindow(QSqlDatabase database, bool demo, QWidget *parent)
    : QMainWindow(parent), store(database), db(database) {
    ok = store.init(demo);
    setWindowTitle("AUREA — Gestion de la production");

    auto *root = new QWidget;
    root->setObjectName("root");
    auto *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ------------------------------------------------------------ sidebar
    auto *side = new QFrame;
    side->setObjectName("sidebar");
    side->setFixedWidth(224);
    auto *sl = new QVBoxLayout(side);
    sl->setContentsMargins(14, 26, 14, 18);
    sl->setSpacing(4);

    auto *logo = new QLabel;
    logo->setPixmap(QPixmap(":/assets/logo_aurea.png").scaled(76, 76, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    auto *brand = new QLabel("AUREA");
    brand->setObjectName("brand");
    brand->setAlignment(Qt::AlignCenter);
    QFont bf = brand->font(); bf.setLetterSpacing(QFont::AbsoluteSpacing, 4); brand->setFont(bf);
    auto *tag = new QLabel("ATELIER HAUTE COUTURE");
    tag->setObjectName("tagline");
    tag->setAlignment(Qt::AlignCenter);
    QFont tf = tag->font(); tf.setLetterSpacing(QFont::AbsoluteSpacing, 1.2); tag->setFont(tf);
    sl->addWidget(logo);
    sl->addSpacing(6);
    sl->addWidget(brand);
    sl->addWidget(tag);
    sl->addSpacing(34);

    struct Nav { const char *label; Kit::Icon icon; };
    const Nav items[] = {{"Tableau de bord", Kit::Icon::Grid}, {"Tâches", Kit::Icon::Tasks}, {"Planning", Kit::Icon::Calendar},
                         {"Suivi", Kit::Icon::Trend}, {"Statistiques", Kit::Icon::Chart}};
    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    for (int i = 0; i < 5; ++i) {
        auto *b = new QPushButton(QStringLiteral("  ") + QString::fromUtf8(items[i].label));
        b->setProperty("nav", true);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setIcon(Kit::navIcon(items[i].icon, QColor("#cfcabd"), QColor("#1f1b10")));
        b->setIconSize(QSize(20, 20));
        b->setMinimumHeight(44);
        group->addButton(b, i);
        navButtons.append(b);
        sl->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, i] { setPage(i); });
        auto *sc = new QShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(i + 1)), this);
        connect(sc, &QShortcut::activated, this, [this, i] { setPage(i); });
    }
    sl->addStretch();
    auto *help = new QPushButton("  Aide");
    help->setObjectName("help");
    help->setIcon(QIcon(Kit::pixmap(Kit::Icon::Help, 20, QColor("#cfcabd"))));
    help->setIconSize(QSize(20, 20));
    help->setCursor(Qt::PointingHandCursor);
    sl->addWidget(help);
    connect(help, &QPushButton::clicked, this, [this] { showHelp(); });

    // ------------------------------------------------------------ barre du haut
    auto *top = new QFrame;
    top->setObjectName("topbar");
    auto *tl = new QHBoxLayout(top);
    tl->setContentsMargins(28, 22, 28, 12);
    tl->setSpacing(18);
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    title = new QLabel;
    title->setObjectName("pageTitle");
    subtitle = new QLabel;
    subtitle->setObjectName("pageSub");
    titles->addWidget(title);
    titles->addWidget(subtitle);
    search = new QLineEdit;
    search->setObjectName("search");
    search->setPlaceholderText("Rechercher une tâche…");
    search->setMinimumWidth(300);
    search->setMaximumWidth(420);
    search->setMinimumHeight(40);
    search->addAction(QIcon(Kit::pixmap(Kit::Icon::Search, 18, Kit::muted)), QLineEdit::LeadingPosition);
    auto *dateLayout = new QHBoxLayout;
    date = new QLabel;
    dateLayout->addWidget(date);
    auto *dateCard = new QFrame;
    dateCard->setProperty("card", true);
    dateLayout->setContentsMargins(22, 12, 22, 12);
    dateCard->setLayout(dateLayout);
    tl->addLayout(titles);
    tl->addSpacing(14);
    tl->addWidget(search, 1);
    tl->addStretch(1);
    tl->addWidget(dateCard);

    // ------------------------------------------------------------ pages
    dashboard = new DashboardPage;
    tasks = new TasksPage(&store);
    planning = new PlanningPage;
    tracking = new TrackingPage;
    stats = new StatsPage;
    stack = new QStackedWidget;
    for (QWidget *w : QList<QWidget *>{dashboard, tasks, planning, tracking, stats}) stack->addWidget(w);

    auto *footer = new QLabel("Gestion de la production · sauvegarde SQLite immédiate");
    footer->setObjectName("footer");
    footer->setContentsMargins(28, 6, 0, 10);

    auto *right = new QVBoxLayout;
    right->setSpacing(0);
    right->addWidget(top);
    right->addWidget(stack, 1);
    right->addWidget(footer);
    rootLayout->addWidget(side);
    rootLayout->addLayout(right, 1);
    setCentralWidget(root);
    setMinimumSize(1180, 680);

    connect(tasks, &TasksPage::changed, this, [this] { reload(); });
    connect(dashboard, &DashboardPage::lateClicked, this, [this] { tasks->setStateFilter(1); setPage(1); });
    connect(dashboard, &DashboardPage::trackingClicked, this, [this] { setPage(3); });
    connect(search, &QLineEdit::textChanged, this, [this](const QString &s) {
        tasks->setSearch(s);
        if (!s.trimmed().isEmpty() && stack->currentIndex() != 1) setPage(1);
    });
    auto *clock = new QTimer(this);
    connect(clock, &QTimer::timeout, this, [this] { updateDate(); });
    clock->start(20000);
    updateDate();
    if (!ok) QMessageBox::critical(this, "AUREA", "Base de données : " + store.error());
    reload();
    setPage(0);
}

void MainWindow::updateDate() { date->setText(QLocale().toString(QDateTime::currentDateTime(), "dddd d MMMM yyyy · HH:mm")); }

void MainWindow::reload() {
    const QVector<Task> all = store.all();
    dashboard->setTasks(all);
    tasks->setTasks(all);
    planning->setTasks(all);
    tracking->setTasks(all);
    stats->setTasks(all);
}

void MainWindow::setPage(int i) {
    static const QStringList titles = {"Tableau de bord", "Tâches de production", "Planning automatique", "Suivi de l'avancement", "Statistiques"};
    static const QStringList subs = {"La production de votre atelier, au quotidien.",
                                     "Créer, modifier et suivre les tâches de l'atelier.",
                                     "Priorité puis date de livraison : l'ordre de travail se calcule seul.",
                                     "Progression par commande et détection de l'étape qui ralentit l'atelier.",
                                     "Comprendre la répartition de la production."};
    if (i < 0 || i >= titles.size()) return;
    stack->setCurrentIndex(i);
    title->setText(titles[i]);
    subtitle->setText(subs[i]);
    navButtons[i]->setChecked(true);
}

void MainWindow::showHelp() {
    QMessageBox::information(this, "AUREA — Guide",
        "Tâches : ajouter, modifier, supprimer (boutons sur chaque ligne), rechercher, filtrer, trier et exporter en PDF.\n\n"
        "Planning : les tâches non terminées sont classées par priorité puis par date de livraison, et placées sur le calendrier.\n\n"
        "Suivi : progression par commande ; l'étape qui ralentit l'atelier est celle dont le retard moyen est le plus élevé.\n\n"
        "Employés : si la base contient la table du module Gestion des employés, les tâches sont affectées à ces employés "
        "(bouton « Suggérer » dans le formulaire).\n\n"
        "Ctrl+1 à Ctrl+5 : changer de page.\n\nBase active : " + db.databaseName());
}

void MainWindow::renderScreenshots(const QString &dir) {
    QDir().mkpath(dir);
    resize(1500, 920);
    const QStringList names = {"01-tableau-de-bord", "02-taches", "03-planning", "04-suivi", "05-statistiques"};
    for (int i = 0; i < names.size(); ++i) {
        setPage(i);
        QApplication::processEvents();
        grab().save(QDir(dir).filePath(names[i] + ".png"));
    }
}
