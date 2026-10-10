#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDate>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHeaderView>
#include <QIcon>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

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

static const int VIP_THRESHOLD = 100;                 // score à partir duquel le client devient VIP
static const QColor GOLD("#e8d5a0");
static const QColor WINE("#7a1f35");
static const QColor TEXT("#e9e4d8");

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
    t->verticalHeader()->setDefaultSectionSize(40);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->setShowGrid(false);
}

// ============================================================ constructor
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

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
    connect(ui->lineSearch, &QLineEdit::textChanged, this, &MainWindow::refreshClients);
    connect(ui->comboFiltre, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshClients);
    connect(ui->comboTri, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshClients);
    connect(ui->comboClientMes, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshMesures);
    connect(ui->tableClients, &QTableWidget::cellDoubleClicked, this, [this] { on_btnModifier_clicked(); });

    refreshClients();
    showPage(0);

    // Anniversaires du jour : notification au démarrage
    QTimer::singleShot(700, this, [this] {
        const QStringList b = birthdaysToday();
        if (!b.isEmpty())
            QMessageBox::information(this, "Anniversaires du jour",
                "Aujourd'hui, c'est l'anniversaire de :\n\n  • " + b.join("\n  • ")
                + "\n\nVous pouvez leur envoyer un mail depuis « Fidélité & VIP ».");
    });
}

MainWindow::~MainWindow() { delete ui; }

// ============================================================ setup
void MainWindow::setupLogo()
{
    // Logo transparent (fond blanc retiré) affiché directement sur la barre latérale sombre
    const QPixmap logo = QPixmap(":/logo.png")
                             .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->logoLabel->setPixmap(logo);
    setWindowIcon(QIcon(logo));
}

void MainWindow::setupTables()
{
    setupTable(ui->tableClients, { "ID", "Nom", "Mail", "Téléphone", "Naissance", "Mensuration", "Catégorie", "Score" });
    setupTable(ui->tableMesures, { "Date", "Poitrine", "Taille", "Hanches", "Épaules", "Manche", "Hauteur" });
    setupTable(ui->tableFidelite, { "Client", "Commandes", "Montant total", "Score", "Niveau", "Catégorie" });
}

void MainWindow::setupCombos()
{
    ui->comboFiltre->addItems({ "Toutes les catégories", "Standard", "VIP" });
    ui->comboTri->addItems({ "Nom : A → Z", "Nom : Z → A", "Score fidélité ↓", "Date de naissance", "ID" });
    ui->comboCategorie->addItems({ "Standard", "VIP" });
}

void MainWindow::reloadClientCombos()
{
    for (QComboBox *cb : { ui->comboClientMes, ui->comboClientCmd }) {
        QSignalBlocker block(cb);
        const int cur = cb->currentData().toInt();
        cb->clear();
        QSqlQuery q("SELECT id, nom FROM clients ORDER BY nom COLLATE NOCASE");
        while (q.next())
            cb->addItem(q.value(1).toString(), q.value(0));
        const int idx = cb->findData(cur);
        cb->setCurrentIndex(idx < 0 ? 0 : idx);
    }
}

// ============================================================ navigation
void MainWindow::showPage(int index)
{
    static const QStringList names = { "CLIENTS", "AJOUTER UN CLIENT", "FICHE DE MENSURATIONS",
                                       "FIDÉLITÉ & VIP", "STATISTIQUES" };
    m_nav[index]->setChecked(true);
    ui->stackedWidget->setCurrentIndex(index);
    ui->lblBreadcrumb->setText("ATELIER / " + names[index]);

    switch (index) {
    case 0: refreshClients();  break;
    case 2: refreshMesures();  break;
    case 3: refreshFidelite(); break;
    case 4: buildCharts();     break;
    default: break;
    }
}

int MainWindow::currentId(QTableWidget *t) const
{
    const int r = t->currentRow();
    return (r < 0 || !t->item(r, 0)) ? -1 : t->item(r, 0)->data(Qt::UserRole).toInt();
}

// ============================================================ CRUD : afficher / rechercher / trier
void MainWindow::refreshClients()
{
    QString sql = "SELECT id, nom, mail, tel, naissance, mensuration, categorie, score FROM clients "
                  "WHERE (nom LIKE :s OR mail LIKE :s OR tel LIKE :s OR CAST(id AS TEXT) LIKE :s)";
    const bool filtered = ui->comboFiltre->currentIndex() > 0;
    if (filtered) sql += " AND categorie = :c";

    static const QStringList orders = { "nom COLLATE NOCASE ASC", "nom COLLATE NOCASE DESC",
                                        "score DESC", "naissance ASC", "id ASC" };
    sql += " ORDER BY " + orders.value(qMax(0, ui->comboTri->currentIndex()));

    QSqlQuery q;
    q.prepare(sql);
    q.bindValue(":s", "%" + ui->lineSearch->text().trimmed() + "%");
    if (filtered) q.bindValue(":c", ui->comboFiltre->currentText());
    q.exec();

    QTableWidget *t = ui->tableClients;
    t->setRowCount(0);
    while (q.next()) {
        const int r = t->rowCount();
        t->insertRow(r);
        fillRow(t, r, q.value(0).toInt(),
                { q.value(0).toString(), q.value(1).toString(), q.value(2).toString(), q.value(3).toString(),
                  fr(q.value(4).toString()), q.value(5).toString(), q.value(6).toString(), q.value(7).toString() });
        if (q.value(6).toString() == "VIP")
            t->item(r, 6)->setForeground(GOLD);
    }
    ui->lblCount->setText(QString("%1 client(s) affiché(s)").arg(t->rowCount()));

    // KPI
    QSqlQuery k("SELECT COUNT(*), COALESCE(SUM(categorie='VIP'),0), "
                "COALESCE(SUM(strftime('%m',naissance)=strftime('%m','now','localtime')),0), "
                "COALESCE(AVG(score),0) FROM clients");
    if (k.next()) {
        ui->kpiTotal->setText(k.value(0).toString());
        ui->kpiVip->setText(k.value(1).toString());
        ui->kpiAnniv->setText(k.value(2).toString());
        ui->kpiScore->setText(QString::number(k.value(3).toDouble(), 'f', 0));
    }
    reloadClientCombos();
}

// ============================================================ CRUD : ajouter / modifier
void MainWindow::startAdd()
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

void MainWindow::startEdit(int id)
{
    QSqlQuery q;
    q.prepare("SELECT nom, mail, tel, naissance, mensuration, categorie FROM clients WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec() || !q.next()) return;

    m_editId = id;
    ui->lblFormTitle->setText("Modifier le client");
    ui->labelId->setText(QString::number(id));
    ui->leNom->setText(q.value(0).toString());
    ui->leMail->setText(q.value(1).toString());
    ui->leTel->setText(q.value(2).toString());
    ui->deNaissance->setDate(QDate::fromString(q.value(3).toString(), Qt::ISODate));
    ui->leMensuration->setText(q.value(4).toString());
    ui->comboCategorie->setCurrentText(q.value(5).toString());
}

void MainWindow::on_btnNouveau_clicked()
{
    startAdd();
    showPage(1);
}

void MainWindow::on_btnModifier_clicked()
{
    const int id = currentId(ui->tableClients);
    if (id < 0) {
        QMessageBox::information(this, "AUREA", "Sélectionnez d'abord un client dans la liste.");
        return;
    }
    startEdit(id);
    showPage(1);
}

void MainWindow::on_btnAnnuler_clicked() { showPage(0); }

void MainWindow::on_btnEnregistrer_clicked()
{
    const QString nom  = ui->leNom->text().trimmed();
    const QString mail = ui->leMail->text().trimmed();
    const QString tel  = ui->leTel->text().trimmed();

    static const QRegularExpression rxMail(R"(^[\w.+-]+@[\w-]+\.[\w.-]+$)");
    static const QRegularExpression rxTel(R"(^\+?\d{8,12}$)");

    if (nom.isEmpty())                       { QMessageBox::warning(this, "Contrôle de saisie", "Le nom est obligatoire."); return; }
    if (!rxMail.match(mail).hasMatch())      { QMessageBox::warning(this, "Contrôle de saisie", "Adresse mail invalide."); return; }
    if (!rxTel.match(tel).hasMatch())        { QMessageBox::warning(this, "Contrôle de saisie", "Téléphone invalide (8 à 12 chiffres)."); return; }
    if (ui->deNaissance->date() >= QDate::currentDate()) {
        QMessageBox::warning(this, "Contrôle de saisie", "La date de naissance doit être dans le passé.");
        return;
    }

    QSqlQuery q;
    if (m_editId < 0)
        q.prepare("INSERT INTO clients(nom, mail, tel, naissance, mensuration, categorie) VALUES(?,?,?,?,?,?)");
    else
        q.prepare("UPDATE clients SET nom=?, mail=?, tel=?, naissance=?, mensuration=?, categorie=? WHERE id=?");
    q.addBindValue(nom);
    q.addBindValue(mail);
    q.addBindValue(tel);
    q.addBindValue(ui->deNaissance->date().toString(Qt::ISODate));
    q.addBindValue(ui->leMensuration->text().trimmed());
    q.addBindValue(ui->comboCategorie->currentText());
    if (m_editId >= 0) q.addBindValue(m_editId);

    if (!q.exec()) {
        QMessageBox::critical(this, "Erreur base de données", q.lastError().text());
        return;
    }
    QMessageBox::information(this, "AUREA", m_editId < 0 ? "Client ajouté avec succès." : "Client modifié avec succès.");
    showPage(0);
}

// ============================================================ CRUD : supprimer
void MainWindow::on_btnSupprimer_clicked()
{
    const int id = currentId(ui->tableClients);
    if (id < 0) {
        QMessageBox::information(this, "AUREA", "Sélectionnez d'abord un client dans la liste.");
        return;
    }
    const QString nom = ui->tableClients->item(ui->tableClients->currentRow(), 1)->text();
    if (QMessageBox::question(this, "Supprimer", "Supprimer définitivement « " + nom + " » et tout son historique ?")
        != QMessageBox::Yes)
        return;

    for (const QString &table : { "mesures", "commandes" }) {
        QSqlQuery d;
        d.prepare("DELETE FROM " + table + " WHERE client_id = ?");
        d.addBindValue(id);
        d.exec();
    }
    QSqlQuery q;
    q.prepare("DELETE FROM clients WHERE id = ?");
    q.addBindValue(id);
    q.exec();
    refreshClients();
}

void MainWindow::on_btnVoirMesures_clicked()
{
    const int id = currentId(ui->tableClients);
    if (id < 0) {
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
void MainWindow::on_btnExportPdf_clicked()
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
void MainWindow::openMail(const QString &to, const QString &subject, const QString &body)
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
void MainWindow::refreshMesures()
{
    const int id = ui->comboClientMes->currentData().toInt();
    QTableWidget *t = ui->tableMesures;
    t->setRowCount(0);

    QSqlQuery q;
    q.prepare("SELECT date, poitrine, taille, hanches, epaules, manche, hauteur FROM mesures "
              "WHERE client_id = ? ORDER BY date DESC, id DESC");
    q.addBindValue(id);
    q.exec();

    bool first = true;
    while (q.next()) {
        const int r = t->rowCount();
        t->insertRow(r);
        QStringList v = { fr(q.value(0).toString()) };
        for (int c = 1; c <= 6; ++c)
            v << QString::number(q.value(c).toDouble(), 'f', 1) + " cm";
        fillRow(t, r, id, v);
        if (first) {   // pré-remplit les champs avec la dernière prise de mesures
            ui->sbPoitrine->setValue(q.value(1).toDouble());
            ui->sbTaille->setValue(q.value(2).toDouble());
            ui->sbHanches->setValue(q.value(3).toDouble());
            ui->sbEpaules->setValue(q.value(4).toDouble());
            ui->sbManche->setValue(q.value(5).toDouble());
            ui->sbHauteur->setValue(q.value(6).toDouble());
            first = false;
        }
    }
}

void MainWindow::on_btnAjouterMesure_clicked()
{
    const int id = ui->comboClientMes->currentData().toInt();
    if (ui->comboClientMes->count() == 0) return;

    QSqlQuery q;
    q.prepare("INSERT INTO mesures(client_id, date, poitrine, taille, hanches, epaules, manche, hauteur) "
              "VALUES(?,?,?,?,?,?,?,?)");
    q.addBindValue(id);
    q.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    q.addBindValue(ui->sbPoitrine->value());
    q.addBindValue(ui->sbTaille->value());
    q.addBindValue(ui->sbHanches->value());
    q.addBindValue(ui->sbEpaules->value());
    q.addBindValue(ui->sbManche->value());
    q.addBindValue(ui->sbHauteur->value());
    if (!q.exec()) {
        QMessageBox::critical(this, "Erreur", q.lastError().text());
        return;
    }

    // met à jour le résumé "mensuration" du client
    QSqlQuery u;
    u.prepare("UPDATE clients SET mensuration = ? WHERE id = ?");
    u.addBindValue(QString("P%1 / T%2 / H%3").arg(ui->sbPoitrine->value(), 0, 'f', 0)
                       .arg(ui->sbTaille->value(), 0, 'f', 0).arg(ui->sbHanches->value(), 0, 'f', 0));
    u.addBindValue(id);
    u.exec();

    refreshMesures();
    QMessageBox::information(this, "AUREA", "Mesures enregistrées dans l'historique.");
}

void MainWindow::on_btnRappel_clicked()
{
    const int id = ui->comboClientMes->currentData().toInt();
    QSqlQuery q;
    q.prepare("SELECT nom, mail FROM clients WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec() || !q.next()) return;

    const QString when = QLocale(QLocale::French).toString(ui->dtEssayage->dateTime(), "dddd d MMMM yyyy 'à' HH:mm");
    openMail(q.value(1).toString(),
             "AUREA — Rappel de votre essayage",
             "Bonjour " + q.value(0).toString() + ",\n\n"
             "Nous vous rappelons votre rendez-vous d'essayage à l'atelier AUREA :\n\n  " + when +
             "\n\nN'hésitez pas à nous contacter pour toute modification.\n\nÀ très bientôt,\nL'équipe AUREA");
}

// ============================================================ Fidélité
bool MainWindow::recomputeScore(int clientId)
{
    QSqlQuery q;
    q.prepare("SELECT COALESCE(SUM(montant),0), "
              "COUNT(CASE WHEN date >= date('now','-1 year') THEN 1 END) FROM commandes WHERE client_id = ?");
    q.addBindValue(clientId);
    q.exec();
    q.next();
    const int score = int(q.value(0).toDouble() / 100.0) + 10 * q.value(1).toInt();

    QSqlQuery u;
    u.prepare("UPDATE clients SET score = ? WHERE id = ?");
    u.addBindValue(score);
    u.addBindValue(clientId);
    u.exec();

    if (score >= VIP_THRESHOLD) {
        QSqlQuery v;
        v.prepare("UPDATE clients SET categorie='VIP' WHERE id = ? AND categorie <> 'VIP'");
        v.addBindValue(clientId);
        v.exec();
        return v.numRowsAffected() > 0;
    }
    return false;
}

void MainWindow::on_btnAjouterCommande_clicked()
{
    if (ui->comboClientCmd->count() == 0) return;
    const int id = ui->comboClientCmd->currentData().toInt();
    if (ui->sbMontant->value() <= 0) {
        QMessageBox::warning(this, "Contrôle de saisie", "Le montant doit être supérieur à 0.");
        return;
    }
    QSqlQuery q;
    q.prepare("INSERT INTO commandes(client_id, montant, date) VALUES(?,?,?)");
    q.addBindValue(id);
    q.addBindValue(ui->sbMontant->value());
    q.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    q.exec();

    const bool nowVip = recomputeScore(id);
    refreshFidelite();
    refreshClients();
    if (nowVip)
        QMessageBox::information(this, "Statut VIP",
                                 ui->comboClientCmd->currentText() + " vient de devenir client VIP !");
}

QStringList MainWindow::birthdaysToday() const
{
    QStringList names;
    QSqlQuery q("SELECT nom FROM clients WHERE strftime('%m-%d', naissance) = strftime('%m-%d','now','localtime')");
    while (q.next()) names << q.value(0).toString();
    return names;
}

void MainWindow::refreshFidelite()
{
    QTableWidget *t = ui->tableFidelite;
    t->setRowCount(0);
    QSqlQuery q("SELECT c.id, c.nom, COUNT(o.id), COALESCE(SUM(o.montant),0), c.score, c.categorie "
                "FROM clients c LEFT JOIN commandes o ON o.client_id = c.id "
                "GROUP BY c.id ORDER BY c.score DESC");
    while (q.next()) {
        const int score = q.value(4).toInt();
        const QString level = score >= VIP_THRESHOLD ? "Or" : (score >= 50 ? "Argent" : "Bronze");
        const int r = t->rowCount();
        t->insertRow(r);
        fillRow(t, r, q.value(0).toInt(),
                { q.value(1).toString(), q.value(2).toString(),
                  QString::number(q.value(3).toDouble(), 'f', 2) + " DT",
                  QString::number(score), level, q.value(5).toString() });
        if (q.value(5).toString() == "VIP") t->item(r, 5)->setForeground(GOLD);
    }

    const QStringList b = birthdaysToday();
    ui->labelAnniv->setText(b.isEmpty() ? "Aucun anniversaire aujourd'hui."
                                        : "Anniversaire aujourd'hui : " + b.join(", "));
}

void MainWindow::on_btnAnniv_clicked()
{
    const int id = currentId(ui->tableFidelite);
    if (id < 0) {
        QMessageBox::information(this, "AUREA", "Sélectionnez un client dans le classement.");
        return;
    }
    QSqlQuery q;
    q.prepare("SELECT nom, mail, naissance FROM clients WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec() || !q.next()) return;

    const QDate bd = QDate::fromString(q.value(2).toString(), Qt::ISODate);
    const QDate now = QDate::currentDate();
    if (bd.month() != now.month() || bd.day() != now.day()) {
        if (QMessageBox::question(this, "Anniversaire", "Ce n'est pas son anniversaire aujourd'hui. Envoyer quand même ?")
            != QMessageBox::Yes)
            return;
    }
    openMail(q.value(1).toString(),
             "Joyeux anniversaire de la part d'AUREA",
             "Chère " + q.value(0).toString() + ",\n\n"
             "Toute l'équipe de l'atelier AUREA vous souhaite un merveilleux anniversaire.\n"
             "Pour l'occasion, nous vous réservons une attention particulière lors de votre prochaine visite.\n\n"
             "Avec toute notre affection,\nL'équipe AUREA");
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

void MainWindow::buildCharts()
{
    // vide l'ancien contenu
    while (QLayoutItem *it = ui->chartsLayout->takeAt(0)) {
        delete it->widget();
        delete it;
    }

    // ---- 1) Camembert : Standard / VIP
    auto *pie = new QPieSeries;
    QSqlQuery q("SELECT categorie, COUNT(*) FROM clients GROUP BY categorie");
    while (q.next()) {
        QPieSlice *s = pie->append(q.value(0).toString() + " (" + q.value(1).toString() + ")", q.value(1).toInt());
        s->setLabelVisible(true);
        s->setLabelColor(TEXT);
        s->setBrush(q.value(0).toString() == "VIP" ? GOLD : WINE);
    }
    auto *chart1 = new QChart;
    chart1->addSeries(pie);
    styleChart(chart1, "Répartition par catégorie");

    // ---- 2) Histogramme : top 5 scores de fidélité
    auto *set = new QBarSet("Score");
    set->setColor(GOLD);
    QStringList names;
    QSqlQuery top("SELECT nom, score FROM clients ORDER BY score DESC LIMIT 5");
    while (top.next()) {
        names << top.value(0).toString().split(' ').first();
        *set << top.value(1).toInt();
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
    axisY->setGridLineColor(QColor("#3a4045"));
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
