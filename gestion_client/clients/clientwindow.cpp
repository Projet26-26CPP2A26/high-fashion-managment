#include "clientwindow.h"
#include "ui_clientwindow.h"
#include "client.h"
#include "icons.h"

#include <QDate>
#include <QDebug>
#include <QDialog>
#include <QFile>
#include <QDesktopServices>
#include <QFileDialog>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QHeaderView>
#include <QIcon>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTextDocument>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QValueAxis>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QT_CHARTS_USE_NAMESPACE
#endif

static const int VIP_THRESHOLD = Client::SEUIL_VIP;   // score à partir duquel le client devient VIP
static const QColor GOLD("#e8d5a0");
static const QColor WINE("#55555d");   // catégorie « Standard » : gris neutre à côté de l'or VIP
static const QColor TEXT("#d9d5cb");

static QString fr(const QString &iso)
{
    const QDate d = QDate::fromString(iso, Qt::ISODate);
    return d.isValid() ? d.toString("dd/MM/yyyy") : iso;
}

static void fillRow(QTableWidget *t, int row, int id, const QStringList &values)
{
    for (int c = 0; c < values.size(); ++c) {
        auto *it = new QTableWidgetItem(values[c]);
        if (c == 0) it->setData(Qt::UserRole, id);
        t->setItem(row, c, it);
    }
}

static void setupTable(QTableWidget *t, const QStringList &headers)
{
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(42);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->setShowGrid(false);
}

// ============================================================ constructor
ClientWindow::ClientWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::ClientWindow)
{
    ui->setupUi(this);

    // Le style est propre à ce module (il ne modifie pas celui du module Employés).
    QFile css(":/clients/style.qss");
    if (css.open(QFile::ReadOnly))
        setStyleSheet(QString::fromUtf8(css.readAll()));

    setupLogo();
    setupTables();
    setupCombos();

    ui->lblDate->setText(QLocale(QLocale::French).toString(QDate::currentDate(), "dddd d MMMM yyyy"));
    ui->dtEssayage->setDateTime(QDateTime::currentDateTime().addDays(2));
    ui->deNaissance->setDate(QDate(2000, 1, 1));

    // Sidebar navigation
    m_nav = { ui->btnNavClients, ui->btnNavAjouter, ui->btnNavMesures, ui->btnNavFidelite, ui->btnNavStats };
    for (int i = 0; i < m_nav.size(); ++i) {
        m_nav[i]->setProperty("nav", true);
        m_nav[i]->setCheckable(true);
        m_nav[i]->setAutoExclusive(true);
        connect(m_nav[i], &QPushButton::clicked, this, [this, i] {
            if (i == 1) startAdd();
            showPage(i);
        });
    }

    // Live search / filter / sort
    connect(ui->lineSearch, &QLineEdit::textChanged, this, &ClientWindow::refreshClients);
    connect(ui->comboFiltre, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ClientWindow::refreshClients);
    connect(ui->comboTri, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ClientWindow::refreshClients);
    connect(ui->comboClientMes, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ClientWindow::refreshMesures);
    connect(ui->tableClients, &QTableWidget::cellDoubleClicked, this, [this] { on_btnModifier_clicked(); });

    refreshClients();
    showPage(0);
}

ClientWindow::~ClientWindow() { delete ui; }

void ClientWindow::setSidebarVisible(bool visible)
{
    ui->sidebar->setVisible(visible);
}

// ============================================================ setup
void ClientWindow::setupLogo()
{
    // Logo transparent (fond blanc retiré) affiché directement sur la barre latérale sombre
    const QPixmap logo = QPixmap(":/clients/logo.png")
                             .scaled(76, 76, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->logoLabel->setPixmap(logo);

    // Icônes dessinées en code : menu latéral (claires, foncées sur le bouton doré) et cartes KPI
    const Icons::Kind kinds[] = { Icons::People, Icons::AddPerson, Icons::Ruler, Icons::Star, Icons::Chart };
    const QList<QPushButton *> nav = { ui->btnNavClients, ui->btnNavAjouter, ui->btnNavMesures, ui->btnNavFidelite, ui->btnNavStats };
    for (int i = 0; i < nav.size(); ++i) {
        nav[i]->setIcon(Icons::nav(kinds[i]));
        nav[i]->setIconSize(QSize(20, 20));
        nav[i]->setCursor(Qt::PointingHandCursor);
    }
    const QColor gold("#e2cf9f");
    ui->lineSearch->addAction(QIcon(Icons::pixmap(Icons::Search, QColor("#8f8c86"), 16)), QLineEdit::LeadingPosition);
    ui->iconTotal->setPixmap(Icons::pixmap(Icons::People, gold, 18));
    ui->iconVip->setPixmap(Icons::pixmap(Icons::Star, gold, 18));
    ui->iconInactifs->setPixmap(Icons::pixmap(Icons::Clock, gold, 18));
    ui->iconScore->setPixmap(Icons::pixmap(Icons::Chart, gold, 18));
}

void ClientWindow::setupTables()
{
    setupTable(ui->tableClients, { "ID", "Nom", "Mail", "Téléphone", "Naissance", "Mensuration", "Catégorie", "Score" });
    setupTable(ui->tableMesures, { "Date", "Poitrine", "Taille", "Hanches", "Épaules", "Manche", "Hauteur" });
    setupTable(ui->tableFidelite, { "Client", "Commandes", "Montant total", "Score", "Niveau", "Catégorie" });
}

void ClientWindow::setupCombos()
{
    ui->comboFiltre->addItems({ "Toutes les catégories", "Standard", "VIP" });
    ui->comboTri->addItems({ "Nom : A → Z", "Nom : Z → A", "Score fidélité ↓", "Date de naissance", "ID" });
    ui->comboCategorie->addItems({ "Standard", "VIP" });
}

void ClientWindow::reloadClientCombos()
{
    const QList<QPair<int, QString>> clients = Client::listeClients();
    for (QComboBox *cb : { ui->comboClientMes, ui->comboClientCmd }) {
        QSignalBlocker block(cb);
        const int cur = cb->currentData().toInt();
        cb->clear();
        for (const auto &c : clients)
            cb->addItem(c.second, c.first);
        const int idx = cb->findData(cur);
        cb->setCurrentIndex(idx < 0 ? 0 : idx);
    }
}

// ============================================================ navigation
void ClientWindow::showPage(int index)
{
    static const QStringList names = { "CLIENTS", "AJOUTER UN CLIENT", "FICHE DE MENSURATIONS",
                                       "FIDÉLITÉ & VIP", "STATISTIQUES" };
    m_nav[index]->setChecked(true);
    const bool changed = ui->stackedWidget->currentIndex() != index;
    ui->stackedWidget->setCurrentIndex(index);
    if (changed) {   // fondu doux de 200 ms à chaque changement de page
        QWidget *page = ui->stackedWidget->currentWidget();
        auto *fade = new QGraphicsOpacityEffect(page);
        page->setGraphicsEffect(fade);
        auto *anim = new QPropertyAnimation(fade, "opacity", page);
        anim->setDuration(200);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QPropertyAnimation::finished, page, [page] { page->setGraphicsEffect(nullptr); });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
    ui->lblBreadcrumb->setText("ATELIER / " + names[index]);

    switch (index) {
    case 0: refreshClients();  break;
    case 2: refreshMesures();  break;
    case 3: refreshFidelite(); break;
    case 4: buildCharts();     break;
    default: break;
    }
}

int ClientWindow::currentId(QTableWidget *t) const
{
    const int r = t->currentRow();
    return (r < 0 || !t->item(r, 0)) ? -1 : t->item(r, 0)->data(Qt::UserRole).toInt();
}

// ============================================================ CRUD : afficher / rechercher / trier
void ClientWindow::refreshClients()
{
    const QString categorie = ui->comboFiltre->currentIndex() > 0 ? ui->comboFiltre->currentText() : QString();
    const QList<Client> liste = Client::afficher(ui->lineSearch->text().trimmed(),
                                                 categorie,
                                                 qMax(0, ui->comboTri->currentIndex()));

    QTableWidget *t = ui->tableClients;
    t->setRowCount(0);
    for (const Client &c : liste) {
        const int r = t->rowCount();
        t->insertRow(r);
        fillRow(t, r, c.getID(),
                { QString::number(c.getID()), c.getNom(), c.getMail(), c.getTel(),
                  fr(c.getNaissance()), c.getMensuration(), c.getCategorie(), QString::number(c.getScore()) });
        if (c.getCategorie() == "VIP")
            t->item(r, 6)->setForeground(GOLD);
    }
    ui->lblCount->setText(QString("%1 client(s) affiché(s)").arg(t->rowCount()));

    // KPI
    const Client::Stats s = Client::statistiques();
    ui->kpiTotal->setText(QString::number(s.total));
    ui->kpiVip->setText(QString::number(s.vip));
    ui->kpiInactifs->setText(QString::number(s.inactifs));
    ui->kpiScore->setText(QString::number(s.scoreMoyen, 'f', 0));

    reloadClientCombos();
}

// ============================================================ CRUD : ajouter / modifier
void ClientWindow::startAdd()
{
    m_editId = -1;
    ui->lblFormTitle->setText("Ajouter un client");
    ui->labelId->setText("Automatique");
    ui->leNom->clear();
    ui->leMail->clear();
    ui->leTel->clear();
    ui->leMensuration->clear();
    ui->deNaissance->setDate(QDate(2000, 1, 1));
    ui->comboCategorie->setCurrentIndex(0);
}

bool ClientWindow::startEdit(int id)
{
    Client c;
    if (!Client::charger(id, c)) {
        QMessageBox::critical(this, "Erreur",
                              "Impossible de charger le client " + QString::number(id)
                              + "\n" + Client::derniereErreur());
        return false;
    }

    m_editId = id;
    qDebug() << "Mode modification, m_editId =" << m_editId;
    ui->lblFormTitle->setText("Modifier le client");
    ui->labelId->setText(QString::number(id));
    ui->leNom->setText(c.getNom());
    ui->leMail->setText(c.getMail());
    ui->leTel->setText(c.getTel());
    ui->deNaissance->setDate(QDate::fromString(c.getNaissance(), Qt::ISODate));
    ui->leMensuration->setText(c.getMensuration());
    ui->comboCategorie->setCurrentText(c.getCategorie());
    return true;
}

void ClientWindow::on_btnNouveau_clicked()
{
    startAdd();
    showPage(1);
}

void ClientWindow::on_btnModifier_clicked()
{
    const int id = currentId(ui->tableClients);
    qDebug() << "Modifier : id lu dans le tableau =" << id;
    if (id <= 0) {
        QMessageBox::information(this, "AUREA", "Sélectionnez d'abord un client dans la liste.");
        return;
    }
    if (startEdit(id))          // on n'ouvre le formulaire que si le chargement a réussi
        showPage(1);
}

void ClientWindow::on_btnAnnuler_clicked() { showPage(0); }

void ClientWindow::on_btnEnregistrer_clicked()
{
    // 1) la vue lit les champs et remplit l'objet Client
    Client c(m_editId,
             ui->leNom->text().trimmed(),
             ui->leMail->text().trimmed(),
             ui->leTel->text().trimmed(),
             ui->deNaissance->date().toString(Qt::ISODate),
             ui->leMensuration->text().trimmed(),
             ui->comboCategorie->currentText());

    // 2) contrôle de saisie (dans la classe C++)
    QString erreur;
    if (!c.valider(erreur)) {
        QMessageBox::warning(this, "Contrôle de saisie", erreur);
        return;
    }

    // 3) la classe fait le travail SQL
    qDebug() << "Enregistrer : m_editId =" << m_editId << (m_editId < 0 ? "-> AJOUTER" : "-> MODIFIER");
    const bool ok = (m_editId < 0) ? c.ajouter() : c.modifier();
    if (!ok) {
        QMessageBox::critical(this, "Erreur base de données", Client::derniereErreur());
        return;
    }

    QMessageBox::information(this, "AUREA", m_editId < 0 ? "Client ajouté avec succès." : "Client modifié avec succès.");
    m_editId = -1;
    showPage(0);
}

// ============================================================ CRUD : supprimer
void ClientWindow::on_btnSupprimer_clicked()
{
    const int id = currentId(ui->tableClients);
    if (id <= 0) {
        QMessageBox::information(this, "AUREA", "Sélectionnez d'abord un client dans la liste.");
        return;
    }
    const QString nom = ui->tableClients->item(ui->tableClients->currentRow(), 1)->text();
    if (QMessageBox::question(this, "Supprimer", "Supprimer définitivement « " + nom + " » et tout son historique ?")
        != QMessageBox::Yes)
        return;

    if (!Client::supprimer(id)) {
        QMessageBox::critical(this, "Erreur base de données", Client::derniereErreur());
        return;
    }
    refreshClients();
}

void ClientWindow::on_btnVoirMesures_clicked()
{
    const int id = currentId(ui->tableClients);
    if (id <= 0) {
        QMessageBox::information(this, "AUREA", "Sélectionnez d'abord un client dans la liste.");
        return;
    }
    {
        QSignalBlocker b(ui->comboClientMes);
        ui->comboClientMes->setCurrentIndex(ui->comboClientMes->findData(id));
    }
    showPage(2);
}

// ============================================================ Export PDF
void ClientWindow::on_btnExportPdf_clicked()
{
    const QString path = QFileDialog::getSaveFileName(this, "Exporter en PDF", "clients_AUREA.pdf", "PDF (*.pdf)");
    if (path.isEmpty()) return;

    QString html = "<h1 style='color:#7a1f35;'>AUREA — Liste des clients</h1>"
                   "<p>Édité le " + QDate::currentDate().toString("dd/MM/yyyy") + "</p>"
                   "<table border='1' cellspacing='0' cellpadding='5' width='100%'><tr style='background:#e8d5a0;'>";
    QTableWidget *t = ui->tableClients;
    for (int c = 0; c < t->columnCount(); ++c)
        html += "<th>" + t->horizontalHeaderItem(c)->text() + "</th>";
    html += "</tr>";
    for (int r = 0; r < t->rowCount(); ++r) {
        html += "<tr>";
        for (int c = 0; c < t->columnCount(); ++c)
            html += "<td>" + t->item(r, c)->text().toHtmlEscaped() + "</td>";
        html += "</tr>";
    }
    html += "</table>";

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageOrientation(QPageLayout::Landscape);
    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&writer);

    QMessageBox::information(this, "AUREA", "PDF exporté avec succès.");
}

// ============================================================ Mail helper
void ClientWindow::openMail(const QString &to, const QString &subject, const QString &body)
{
    QUrl url;
    url.setScheme("mailto");
    url.setPath(to);
    QUrlQuery q;
    q.addQueryItem("subject", subject);
    q.addQueryItem("body", body);
    url.setQuery(q);
    if (!QDesktopServices::openUrl(url))
        QMessageBox::warning(this, "Mail", "Aucune application de messagerie trouvée.\n\nDestinataire : " + to);
}

// ============================================================ Fiche de mensurations
void ClientWindow::refreshMesures()
{
    const int id = ui->comboClientMes->currentData().toInt();
    QTableWidget *t = ui->tableMesures;
    t->setRowCount(0);

    bool first = true;
    for (const Client::Mesure &m : Client::mesures(id)) {
        const int r = t->rowCount();
        t->insertRow(r);
        QStringList v = { fr(m.date) };
        for (double val : { m.poitrine, m.taille, m.hanches, m.epaules, m.manche, m.hauteur })
            v << QString::number(val, 'f', 1) + " cm";
        fillRow(t, r, id, v);
        if (first) {   // pré-remplit les champs avec la dernière prise de mesures
            ui->sbPoitrine->setValue(m.poitrine);
            ui->sbTaille->setValue(m.taille);
            ui->sbHanches->setValue(m.hanches);
            ui->sbEpaules->setValue(m.epaules);
            ui->sbManche->setValue(m.manche);
            ui->sbHauteur->setValue(m.hauteur);
            first = false;
        }
    }
}

void ClientWindow::on_btnAjouterMesure_clicked()
{
    if (ui->comboClientMes->count() == 0) return;
    const int id = ui->comboClientMes->currentData().toInt();

    Client::Mesure m;
    m.poitrine = ui->sbPoitrine->value();
    m.taille = ui->sbTaille->value();
    m.hanches = ui->sbHanches->value();
    m.epaules = ui->sbEpaules->value();
    m.manche = ui->sbManche->value();
    m.hauteur = ui->sbHauteur->value();

    if (!Client::ajouterMesure(id, m)) {
        QMessageBox::critical(this, "Erreur", Client::derniereErreur());
        return;
    }

    refreshMesures();
    QMessageBox::information(this, "AUREA", "Mesures enregistrées dans l'historique.");
}

// métier avancé : taille recommandée à partir de la dernière prise de mesures
void ClientWindow::on_btnTaille_clicked()
{
    if (ui->comboClientMes->count() == 0) return;
    QString detail;
    const QString t = Client::tailleRecommandee(ui->comboClientMes->currentData().toInt(), &detail);
    if (t.isEmpty()) {
        QMessageBox::information(this, "AUREA", detail);
        return;
    }
    QMessageBox::information(this, "Taille recommandée",
                             ui->comboClientMes->currentText() + " : taille " + t + "\n\n" + detail);
}

void ClientWindow::on_btnRappel_clicked()
{
    const int id = ui->comboClientMes->currentData().toInt();
    Client c;
    if (!Client::charger(id, c)) return;

    const QString when = QLocale(QLocale::French).toString(ui->dtEssayage->dateTime(), "dddd d MMMM yyyy 'à' HH:mm");
    openMail(c.getMail(),
             "AUREA — Rappel de votre essayage",
             "Bonjour " + c.getNom() + ",\n\n"
             "Nous vous rappelons votre rendez-vous d'essayage à l'atelier AUREA :\n\n  " + when +
             "\n\nN'hésitez pas à nous contacter pour toute modification.\n\nÀ très bientôt,\nL'équipe AUREA");
}

// ============================================================ Fidélité
void ClientWindow::on_btnAjouterCommande_clicked()
{
    if (ui->comboClientCmd->count() == 0) return;
    const int id = ui->comboClientCmd->currentData().toInt();
    if (ui->sbMontant->value() <= 0) {
        QMessageBox::warning(this, "Contrôle de saisie", "Le montant doit être supérieur à 0.");
        return;
    }

    bool nowVip = false;
    if (!Client::ajouterCommande(id, ui->sbMontant->value(), &nowVip)) {
        QMessageBox::critical(this, "Erreur", Client::derniereErreur());
        return;
    }

    refreshFidelite();
    refreshClients();
    if (nowVip)
        QMessageBox::information(this, "Statut VIP",
                                 ui->comboClientCmd->currentText() + " vient de devenir client VIP !");
}

void ClientWindow::refreshFidelite()
{
    QTableWidget *t = ui->tableFidelite;
    t->setRowCount(0);
    for (const Client::LigneFidelite &l : Client::classementFidelite()) {
        const QString level = l.score >= VIP_THRESHOLD ? "Or" : (l.score >= 50 ? "Argent" : "Bronze");
        const int r = t->rowCount();
        t->insertRow(r);
        fillRow(t, r, l.id,
                { l.nom, QString::number(l.nbCommandes),
                  QString::number(l.montant, 'f', 2) + " DT",
                  QString::number(l.score), level, l.categorie });
        if (l.categorie == "VIP") t->item(r, 5)->setForeground(GOLD);
    }
}

// métier avancé : liste des clients inactifs + mail de relance
void ClientWindow::on_btnRelance_clicked()
{
    const QList<Client::ClientInactif> inactifs = Client::clientsInactifs(6);
    if (inactifs.isEmpty()) {
        QMessageBox::information(this, "AUREA", "Aucun client inactif depuis 6 mois.");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Clients inactifs (6 mois)");
    dlg.resize(620, 420);
    auto *lay = new QVBoxLayout(&dlg);
    auto *t = new QTableWidget;
    setupTable(t, { "Client", "Mail", "Dernière commande" });
    for (const auto &c : inactifs) {
        const int r = t->rowCount();
        t->insertRow(r);
        fillRow(t, r, c.id, { c.nom, c.mail, c.derniereCommande.isEmpty() ? "Jamais" : fr(c.derniereCommande) });
    }
    auto *btn = new QPushButton("Envoyer un mail de relance au client sélectionné");
    lay->addWidget(t);
    lay->addWidget(btn);

    connect(btn, &QPushButton::clicked, &dlg, [&] {
        const int r = t->currentRow();
        if (r < 0) {
            QMessageBox::information(&dlg, "AUREA", "Sélectionnez un client dans la liste.");
            return;
        }
        openMail(t->item(r, 1)->text(), "AUREA — Vous nous manquez",
                 "Bonjour " + t->item(r, 0)->text() + ",\n\nCela fait un moment que nous ne vous avons pas vu "
                 "à l'atelier AUREA. Une remise vous attend pour votre prochaine commande.\n\nL'équipe AUREA");
    });
    dlg.exec();
}

// ============================================================ Statistiques (graphiques)
static void styleChart(QChart *chart, const QString &title)
{
    chart->setTitle(title);
    chart->setTitleBrush(QBrush(GOLD));
    chart->setBackgroundVisible(false);
    chart->legend()->setLabelColor(TEXT);
    chart->setAnimationOptions(QChart::SeriesAnimations);
}

void ClientWindow::buildCharts()
{
    // vide l'ancien contenu
    while (QLayoutItem *it = ui->chartsLayout->takeAt(0)) {
        delete it->widget();
        delete it;
    }

    // ---- 1) Camembert : Standard / VIP
    auto *pie = new QPieSeries;
    for (const auto &p : Client::repartitionCategories()) {
        QPieSlice *s = pie->append(p.first + " (" + QString::number(p.second) + ")", p.second);
        s->setLabelVisible(true);
        s->setLabelColor(TEXT);
        s->setBrush(p.first == "VIP" ? GOLD : WINE);
    }
    auto *chart1 = new QChart;
    chart1->addSeries(pie);
    styleChart(chart1, "Répartition par catégorie");

    // ---- 2) Histogramme : top 5 scores de fidélité
    auto *set = new QBarSet("Score");
    set->setColor(GOLD);
    QStringList names;
    for (const auto &p : Client::topScores(5)) {
        names << p.first.split(' ').first();
        *set << p.second;
    }
    auto *bars = new QBarSeries;
    bars->append(set);
    auto *chart2 = new QChart;
    chart2->addSeries(bars);
    styleChart(chart2, "Top 5 — score de fidélité");

    auto *axisX = new QBarCategoryAxis;
    axisX->append(names);
    axisX->setLabelsColor(TEXT);
    auto *axisY = new QValueAxis;
    axisY->setLabelFormat("%d");
    axisY->setLabelsColor(TEXT);
    axisY->setGridLineColor(QColor("#323236"));
    chart2->addAxis(axisX, Qt::AlignBottom);
    chart2->addAxis(axisY, Qt::AlignLeft);
    bars->attachAxis(axisX);
    bars->attachAxis(axisY);

    for (QChart *c : { chart1, chart2 }) {
        auto *view = new QChartView(c);
        view->setRenderHint(QPainter::Antialiasing);
        view->setStyleSheet("background: transparent; border: none;");
        ui->chartsLayout->addWidget(view);
    }
}
