#include "shellwindow.h"
#include "employeewindow.h"
#include "materialwindow.h"
#include "productionwindow.h"
#include "icons.h"
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

ShellWindow::ShellWindow(QSqlDatabase employeeDatabase, InventoryRepository &stock, bool demo, QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("AUREA — Atelier haute couture"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/logo_aurea.png")));
    setMinimumSize(1300, 780);

    QFile css(QStringLiteral(":/shell/shell.qss"));
    if (css.open(QIODevice::ReadOnly)) setStyleSheet(QString::fromUtf8(css.readAll()));

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("shellCentral"));
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---------- colonne de gauche : marque + les deux gestions (verticales) ----------
    auto *side = new QFrame(central);
    side->setObjectName(QStringLiteral("shellSidebar"));
    side->setFixedWidth(256);
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(16, 22, 16, 18);
    sideLayout->setSpacing(6);

    auto *logo = new QLabel(side);
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(QPixmap(QStringLiteral(":/assets/logo_aurea.png")).scaled(70, 70, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    auto *brand = new QLabel(QStringLiteral("AUREA"), side);
    brand->setObjectName(QStringLiteral("shellBrand"));
    brand->setAlignment(Qt::AlignCenter);
    auto *tagline = new QLabel(QStringLiteral("ATELIER HAUTE COUTURE"), side);
    tagline->setObjectName(QStringLiteral("shellTagline"));
    tagline->setAlignment(Qt::AlignCenter);
    sideLayout->addWidget(logo);
    sideLayout->addWidget(brand);
    sideLayout->addWidget(tagline);
    sideLayout->addSpacing(26);
    auto *caption = new QLabel(QStringLiteral("GESTION"), side);
    caption->setObjectName(QStringLiteral("shellCaption"));
    sideLayout->addWidget(caption);

    group = new QButtonGroup(this);
    group->setExclusive(true);
    const struct { QPushButton **target; QString text; Icons::Kind icon; QString tip; } defs[] = {
        {&employeeButton, QStringLiteral("  Gestion des employés"), Icons::Badge,  QStringLiteral("Employés, affectation, pointage (Alt+1)")},
        {&materialButton, QStringLiteral("  Gestion des matières"), Icons::Grid,    QStringLiteral("Stock, réapprovisionnement, lots & QR (Alt+2)")},
        {&productionButton, QStringLiteral("  Gestion de la production"), Icons::Assign, QStringLiteral("Tâches, planning, suivi de l'avancement (Alt+3)")},
    };
    for (int i = 0; i < 3; ++i) {
        auto *button = new QPushButton(defs[i].text, side);
        button->setProperty("module", true);
        button->setCheckable(true);
        button->setIcon(Icons::nav(defs[i].icon));
        button->setIconSize(QSize(20, 20));
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(defs[i].tip);
        group->addButton(button, i);
        sideLayout->addWidget(button);
        *defs[i].target = button;
        connect(button, &QPushButton::clicked, this, [this, i] { setModule(i); });
        auto *shortcut = new QShortcut(QKeySequence(QStringLiteral("Alt+%1").arg(i + 1)), this);
        connect(shortcut, &QShortcut::activated, this, [this, i] { setModule(i); });
    }
    sideLayout->addStretch(1);
    auto *footer = new QLabel(QStringLiteral("AUREA / COLLECTION ATELIER"), side);
    footer->setObjectName(QStringLiteral("shellFooter"));
    footer->setAlignment(Qt::AlignCenter);
    sideLayout->addWidget(footer);
    root->addWidget(side);

    // ---------- zone de droite : boutons du module (horizontaux) puis page ----------
    auto *right = new QWidget(central);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    tabStack = new QStackedWidget(right);
    tabStack->setObjectName(QStringLiteral("shellTabs"));
    tabStack->setFixedHeight(64);
    rightLayout->addWidget(tabStack);

    stack = new QStackedWidget(right);
    stack->setObjectName(QStringLiteral("shellStack"));
    employeeWindow = new EmployeeWindow(employeeDatabase, demo, stack);
    materialWindow = new MaterialWindow(stock, stack);
    productionWindow = new ProductionWindow(employeeDatabase, demo, stack);   // même base que les employés
    // QMainWindow se déclare « fenêtre » par défaut : on l'intègre comme simple widget.
    employeeWindow->setWindowFlags(Qt::Widget);
    materialWindow->setWindowFlags(Qt::Widget);
    productionWindow->setWindowFlags(Qt::Widget);
    employeeWindow->setMinimumSize(0, 0);   // les minimums d'origine incluaient leur barre latérale
    materialWindow->setMinimumSize(0, 0);
    productionWindow->setMinimumSize(0, 0);
    // Leur barre latérale est remplacée par la rangée de boutons du haut.
    employeeWindow->setSidebarVisible(false);
    materialWindow->setSidebarVisible(false);
    productionWindow->setSidebarVisible(false);
    stack->addWidget(employeeWindow);
    stack->addWidget(materialWindow);
    stack->addWidget(productionWindow);
    tabStack->addWidget(makeTabStrip(employeeWindow->navButtons(), employeeWindow->helpButton(), tabStack));
    tabStack->addWidget(makeTabStrip(materialWindow->navButtons(), materialWindow->helpButton(), tabStack));
    tabStack->addWidget(makeTabStrip(productionWindow->navButtons(), productionWindow->helpButton(), tabStack));
    rightLayout->addWidget(stack, 1);
    root->addWidget(right, 1);
    setCentralWidget(central);

    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("shell/geometry")).toByteArray());
    setModule(settings.value(QStringLiteral("shell/activeModule"), Employees).toInt());
}

ShellWindow::~ShellWindow() = default;

// Rangée horizontale de boutons qui pilote les boutons (cachés) du menu latéral d'origine :
// un clic ici active le bouton d'origine, et tout changement de page interne met la rangée à jour.
QWidget *ShellWindow::makeTabStrip(const QList<QPushButton *> &inner, QPushButton *help, QWidget *parent)
{
    auto *strip = new QWidget(parent);
    auto *layout = new QHBoxLayout(strip);
    layout->setContentsMargins(22, 0, 22, 0);
    layout->setSpacing(8);
    auto *buttons = new QButtonGroup(strip);
    buttons->setExclusive(true);
    for (QPushButton *source : inner) {
        auto *tab = new QPushButton(source->text().trimmed(), strip);
        tab->setProperty("tab", true);
        tab->setCheckable(true);
        tab->setIcon(source->icon());
        tab->setIconSize(QSize(18, 18));
        tab->setCursor(Qt::PointingHandCursor);
        tab->setChecked(source->isChecked());
        buttons->addButton(tab);
        layout->addWidget(tab);
        connect(tab, &QPushButton::clicked, source, [source] { source->click(); });
        connect(source, &QPushButton::toggled, tab, [tab](bool on) { if (on) tab->setChecked(true); });
    }
    layout->addStretch(1);
    if (help) {
        auto *aide = new QPushButton(QStringLiteral("Aide"), strip);
        aide->setProperty("tab", true);
        aide->setIcon(help->icon());
        aide->setIconSize(QSize(18, 18));
        aide->setCursor(Qt::PointingHandCursor);
        connect(aide, &QPushButton::clicked, help, [help] { help->click(); });
        layout->addWidget(aide);
    }
    return strip;
}

bool ShellWindow::ready() const { return employeeWindow->ready(); }
int ShellWindow::module() const { return stack->currentIndex(); }

void ShellWindow::setModule(int module)
{
    if (module < 0 || module > 2) module = Employees;
    stack->setCurrentIndex(module);
    tabStack->setCurrentIndex(module);
    group->button(module)->setChecked(true);
}

void ShellWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.setValue(QStringLiteral("shell/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("shell/activeModule"), module());
    QMainWindow::closeEvent(event);
}

void ShellWindow::renderScreenshots(const QString &directory)
{
    QDir().mkpath(directory);
    resize(1440, 900);
    const QStringList names = {QStringLiteral("employes-tableau-de-bord"), QStringLiteral("employes-liste"),
                               QStringLiteral("employes-affectation"), QStringLiteral("employes-pointage"),
                               QStringLiteral("employes-statistiques")};
    setModule(Employees);
    for (int i = 0; i < names.size(); ++i) {
        employeeWindow->setPage(i);
        QApplication::processEvents();
        grab().save(QDir(directory).filePath(QStringLiteral("%1-%2.png").arg(i + 1, 2, 10, QChar('0')).arg(names[i])));
    }
    const QStringList materialNames = {QStringLiteral("matieres-stock"), QStringLiteral("matieres-fiche"), QStringLiteral("matieres-reappro"),
                                       QStringLiteral("matieres-lots-qr"), QStringLiteral("matieres-statistiques"), QStringLiteral("matieres-historique")};
    setModule(Materials);
    const QList<QPushButton *> materialTabs = materialWindow->navButtons();
    for (int i = 0; i < materialNames.size(); ++i) {
        materialTabs[i]->click();
        QElapsedTimer wait; wait.start();            // laisse finir le graphique des statistiques
        while (wait.elapsed() < 400) QApplication::processEvents(QEventLoop::AllEvents, 50);
        grab().save(QDir(directory).filePath(QStringLiteral("%1-%2.png").arg(i + 6, 2, 10, QChar('0')).arg(materialNames[i])));
    }
    const QStringList productionNames = {QStringLiteral("production-tableau-de-bord"), QStringLiteral("production-taches"),
                                         QStringLiteral("production-planning"), QStringLiteral("production-suivi"),
                                         QStringLiteral("production-statistiques")};
    setModule(Production);
    for (int i = 0; i < productionNames.size(); ++i) {
        productionWindow->setPage(i);
        QApplication::processEvents();
        grab().save(QDir(directory).filePath(QStringLiteral("%1-%2.png").arg(i + 12, 2, 10, QChar('0')).arg(productionNames[i])));
    }
}
