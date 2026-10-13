#include "orderwindow.h"
#include "icons.h"
#include "pdfexport.h"

#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QShowEvent>
#include <QSqlError>
#include <QSqlQuery>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

// ============================================================ Constantes (palette AUREA)
static const QStringList kStatuts = {QStringLiteral("En attente"), QStringLiteral("En cours"),
                                     QStringLiteral("Terminée"), QStringLiteral("Livrée")};
static const QColor kGold(0xe2, 0xcf, 0x9f);
static const QColor kVert(0x7f, 0xc0, 0x8a);
static const QColor kOrange(0xe0, 0xb0, 0x62);
static const QColor kRouge(0xd9, 0x8b, 0x8b);
static const QColor kBleu(0x86, 0xa9, 0xc7);
static const QColor kMuted(0x8f, 0x8c, 0x86);
static const QColor kCardBg(0x23, 0x23, 0x26);
static const double kRemiseVip = 10.0;   // % de remise VIP par défaut
static const double kTauxHoraire = 35.0; // DT / heure par défaut

// ============================================================ Utilitaires
static QString fmtMoney(double v, int dec = 2)
{
    return QLocale(QLocale::French).toString(v, 'f', dec) + QStringLiteral(" DT");
}

static QString fmtMoneyAuto(double v)
{
    return fmtMoney(v, std::abs(v - std::round(v)) < 0.005 ? 0 : 2);
}

static QString cmdCode(int id)
{
    return QStringLiteral("CMD-%1").arg(id, 4, 10, QChar('0'));
}

static void setupTable(QTableWidget *t, const QStringList &headers)
{
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(42);
    t->setFrameShape(QFrame::NoFrame);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setShowGrid(false);
    t->setWordWrap(false);
    t->setFocusPolicy(Qt::NoFocus);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->horizontalHeader()->setHighlightSections(false);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

// cellule texte
static QTableWidgetItem *setCell(QTableWidget *t, int row, int col, const QString &text, int id = -1)
{
    auto *it = new QTableWidgetItem(text);
    if (id >= 0) it->setData(Qt::UserRole, id);
    t->setItem(row, col, it);
    return it;
}

// cellule triable sur une clé numérique
static QTableWidgetItem *setKeyCell(QTableWidget *t, int row, int col, const QString &text, double key, int id = -1)
{
    auto *it = new NumericItem(text);
    it->setData(Qt::UserRole + 1, key);
    if (id >= 0) it->setData(Qt::UserRole, id);
    t->setItem(row, col, it);
    return it;
}

static int selectedId(QTableWidget *t)
{
    const int r = t->currentRow();
    if (r < 0 || !t->item(r, 0)) return -1;
    return t->item(r, 0)->data(Qt::UserRole).toInt();
}

static void sqlError(QWidget *parent, const QSqlQuery &q)
{
    QMessageBox::warning(parent, QStringLiteral("AUREA"), q.lastError().text());
}

// Question oui / non avec des libellés français (sans dépendre des traductions de Qt)
static bool askQuestion(QWidget *parent, const QString &title, const QString &text,
                        const QString &yes, const QString &no, bool danger = false)
{
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::NoButton, parent);
    QPushButton *accept = box.addButton(yes, QMessageBox::AcceptRole);
    box.addButton(no, QMessageBox::RejectRole);
    accept->setProperty(danger ? "danger" : "primary", true);
    box.exec();
    return box.clickedButton() == accept;
}

// Carte du tableau de bord : titre (+ widget à droite) puis contenu
static QFrame *makeCard(const QString &title, QLayout **bodyOut, QWidget *rightWidget = nullptr)
{
    auto *f = new QFrame;
    f->setProperty("card", true);
    auto *v = new QVBoxLayout(f);
    v->setContentsMargins(18, 16, 18, 16);
    v->setSpacing(10);

    auto *head = new QHBoxLayout;
    auto *t = new QLabel(title);
    t->setProperty("section", true);
    head->addWidget(t);
    head->addStretch(1);
    if (rightWidget) head->addWidget(rightWidget);
    v->addLayout(head);

    auto *body = new QVBoxLayout;
    body->setSpacing(8);
    v->addLayout(body, 1);
    if (bodyOut) *bodyOut = body;
    return f;
}

static QLabel *makePill(const QString &text)
{
    auto *pill = new QLabel(text);
    pill->setProperty("pill", true);
    return pill;
}

static QFrame *makeStatCard(const QString &title, Icons::Kind icon, QLabel **value, QLabel **sub)
{
    auto *f = new QFrame;
    f->setProperty("card", true);
    auto *g = new QGridLayout(f);
    g->setContentsMargins(16, 14, 16, 14);
    g->setVerticalSpacing(4);

    auto *t = new QLabel(title);
    t->setProperty("muted", true);
    auto *ic = new QLabel;
    ic->setProperty("iconbox", true);
    ic->setFixedSize(30, 30);
    ic->setAlignment(Qt::AlignCenter);
    ic->setPixmap(Icons::pixmap(icon, kGold, 18));

    *value = new QLabel(QStringLiteral("0"));
    (*value)->setProperty("kpi", true);
    *sub = new QLabel;
    (*sub)->setProperty("muted", true);

    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    row->addWidget(*value, 0, Qt::AlignBottom);
    row->addWidget(*sub, 0, Qt::AlignBottom);
    row->addStretch(1);

    g->addWidget(t, 0, 0);
    g->addWidget(ic, 0, 1, Qt::AlignRight);
    g->addLayout(row, 1, 0, 1, 2);
    return f;
}

static QLabel *fieldLabel(const QString &text)
{
    auto *l = new QLabel(text);
    l->setProperty("muted", true);
    return l;
}

// ------------------------------------------------ Suivi des retards
struct Suivi {
    QString texte;
    QColor couleur;
    int niveau; // 0 = vert, 1 = orange, 2 = rouge
};

static Suivi suiviFor(const AtelierOrder &c)
{
    const int jours = QDate::currentDate().daysTo(c.livraison);

    if (c.statut == QStringLiteral("Livrée")) return {QStringLiteral("Livrée"), kVert, 0};
    if (c.statut == QStringLiteral("Terminée")) return {QStringLiteral("Prête à livrer"), kVert, 0};
    if (jours < 0) return {QStringLiteral("Retard de %1 j").arg(-jours), kRouge, 2};

    if (c.statut == QLatin1String("En attente")) {
        // la production n'a pas commencé : le délai fond vite
        if (jours <= 3) return {QStringLiteral("Critique · J-%1").arg(jours), kRouge, 2};
        if (jours <= 7) return {QStringLiteral("À risque · J-%1").arg(jours), kOrange, 1};
        return {QStringLiteral("À temps"), kVert, 0};
    }
    // En cours
    if (jours <= 2) return {QStringLiteral("À risque · J-%1").arg(jours), kOrange, 1};
    return {QStringLiteral("À temps"), kVert, 0};
}

static QColor tint(const QColor &c)
{
    const double a = 0.14;
    return QColor(int(kCardBg.red() * (1 - a) + c.red() * a),
                  int(kCardBg.green() * (1 - a) + c.green() * a),
                  int(kCardBg.blue() * (1 - a) + c.blue() * a));
}

static void tintRow(QTableWidget *t, int row, const QColor &c)
{
    for (int col = 0; col < t->columnCount(); ++col)
        if (t->item(row, col)) t->item(row, col)->setBackground(QBrush(tint(c)));
}

// Les styles QSS des cellules ignorent Qt::BackgroundRole : ce delegate dessine la teinte du suivi.
class TintDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override
    {
        const QVariant bg = idx.data(Qt::BackgroundRole);
        if (bg.isValid() && !(opt.state & QStyle::State_Selected))
            p->fillRect(opt.rect, qvariant_cast<QBrush>(bg));
        QStyledItemDelegate::paint(p, opt, idx);
    }
};

// ------------------------------------------------ Export PDF (même mise en page que les autres modules)
static void exportHtmlToPdf(QWidget *parent, const QString &html, const QString &defaultName, bool landscape)
{
    QString path = QFileDialog::getSaveFileName(parent, QStringLiteral("Enregistrer le PDF"),
                                                QDir::homePath() + "/" + defaultName,
                                                QStringLiteral("Documents PDF (*.pdf)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive)) path += QLatin1String(".pdf");

    QString error;
    if (!Reports::writePdf(path, html, &error, landscape)) {
        QMessageBox::warning(parent, QStringLiteral("AUREA"), error);
        return;
    }
    if (askQuestion(parent, QStringLiteral("Export PDF"),
                    QStringLiteral("PDF enregistré :\n%1\n\nL'ouvrir maintenant ?").arg(path),
                    QStringLiteral("Ouvrir"), QStringLiteral("Plus tard")))
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

static const char *kPdfHead =
    "<html><body style=\"font-family:Inter,'Segoe UI',Arial,sans-serif; color:#222; font-size:9pt;\">"
    "<table width=\"100%\"><tr>"
    "<td><img src=\":/assets/logo_aurea.png\" width=\"90\"></td>"
    "<td align=\"right\"><span style=\"font-size:9pt; color:#777;\">AUREA — Atelier haute couture</span></td>"
    "</tr></table>";

static const char *kPdfHeaderRow = "<tr bgcolor=\"#e5d5a1\">";

// ============================================================ Dialogue commande
static bool commandeDialog(QWidget *parent, const QString &title, AtelierOrder &c, const QVector<AtelierClient> &clients)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setMinimumWidth(480);

    auto *client = new QComboBox;
    for (const AtelierClient &cl : clients)
        client->addItem(QStringLiteral("C%1 · %2%3").arg(cl.id, 3, 10, QChar('0')).arg(cl.nom, cl.vip ? QStringLiteral("  ★ VIP") : QString()),
                        cl.id);
    const int ci = client->findData(c.clientId);
    if (ci >= 0) client->setCurrentIndex(ci);

    auto *modele = new QLineEdit(c.modele);
    modele->setPlaceholderText(QStringLiteral("Ex. Robe de soirée drapée"));

    auto *montant = new QDoubleSpinBox;
    montant->setRange(0, 1000000);
    montant->setDecimals(2);
    montant->setSuffix(QStringLiteral(" DT"));
    montant->setValue(c.montant);

    auto *acompte = new QDoubleSpinBox;
    acompte->setRange(0, 1000000);
    acompte->setDecimals(2);
    acompte->setSuffix(QStringLiteral(" DT"));
    acompte->setValue(c.acompte);

    auto *date = new QDateEdit(c.date);
    date->setCalendarPopup(true);
    date->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    auto *livraison = new QDateEdit(c.livraison);
    livraison->setCalendarPopup(true);
    livraison->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));

    auto *statut = new QComboBox;
    statut->addItems(kStatuts);
    statut->setCurrentText(c.statut);

    auto *error = new QLabel;
    error->setStyleSheet(QStringLiteral("color:#d98b8b;"));
    error->setWordWrap(true);

    auto *form = new QFormLayout;
    form->setVerticalSpacing(12);
    form->setHorizontalSpacing(16);
    form->addRow(QStringLiteral("Client"), client);
    form->addRow(QStringLiteral("Modèle"), modele);
    form->addRow(QStringLiteral("Montant"), montant);
    form->addRow(QStringLiteral("Acompte"), acompte);
    form->addRow(QStringLiteral("Date de commande"), date);
    form->addRow(QStringLiteral("Date de livraison"), livraison);
    form->addRow(QStringLiteral("Statut"), statut);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    box->button(QDialogButtonBox::Ok)->setProperty("primary", true);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Enregistrer"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Annuler"));

    auto *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(22, 22, 22, 18);
    lay->setSpacing(14);
    lay->addLayout(form);
    lay->addWidget(error);
    lay->addWidget(box);

    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, [&]() {
        if (client->currentIndex() < 0) {
            error->setText(QStringLiteral("Choisissez un client."));
            return;
        }
        if (modele->text().trimmed().isEmpty()) {
            error->setText(QStringLiteral("Le modèle est obligatoire."));
            return;
        }
        if (acompte->value() > montant->value()) {
            error->setText(QStringLiteral("L'acompte ne peut pas dépasser le montant."));
            return;
        }
        if (livraison->date() < date->date()) {
            error->setText(QStringLiteral("La livraison doit être après la date de commande."));
            return;
        }
        dlg.accept();
    });

    if (dlg.exec() != QDialog::Accepted) return false;
    c.clientId = client->currentData().toInt();
    c.modele = modele->text().trimmed();
    c.montant = montant->value();
    c.acompte = acompte->value();
    c.date = date->date();
    c.livraison = livraison->date();
    c.statut = statut->currentText();
    return true;
}

// ============================================================ RevenueChart
RevenueChart::RevenueChart(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(220);
}

void RevenueChart::setData(const QStringList &labels, const QVector<double> &values, const QString &unit)
{
    m_labels = labels;
    m_values = values;
    m_unit = unit;
    update();
}

void RevenueChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QFont f = font();
    f.setPointSize(8);
    p.setFont(f);

    double maxV = 0;
    for (double v : m_values) maxV = std::max(maxV, v);

    // pas "agréable" pour l'axe Y (4 intervalles)
    double step = 1;
    if (maxV > 0) {
        const double raw = maxV / 4.0;
        const double mag = std::pow(10.0, std::floor(std::log10(raw)));
        const double norm = raw / mag;
        step = (norm <= 1 ? 1 : norm <= 2 ? 2 : norm <= 5 ? 5 : 10) * mag;
    }
    const double axisMax = step * 4;

    auto yLabel = [&](int i) { return QLocale(QLocale::French).toString(qRound(step * i)) + m_unit; };
    int left = 40;
    for (int i = 0; i <= 4; ++i)
        left = std::max(left, p.fontMetrics().horizontalAdvance(yLabel(i)) + 16);

    const int right = 28, top = 12, bottom = 30;
    const QRectF plot(left, top, width() - left - right, height() - top - bottom);
    const int n = m_values.size();

    for (int i = 0; i <= 4; ++i) {
        const double y = plot.bottom() - plot.height() * i / 4.0;
        p.setPen(QPen(QColor(255, 255, 255, 14), 1));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(kMuted);
        p.drawText(QRectF(0, y - 9, left - 8, 18), Qt::AlignRight | Qt::AlignVCenter, yLabel(i));
    }
    if (n < 2) return;

    QVector<QPointF> pts;
    for (int i = 0; i < n; ++i) {
        const double x = plot.left() + plot.width() * i / (n - 1);
        const double y = plot.bottom() - m_values[i] / axisMax * plot.height();
        pts << QPointF(x, y);
    }

    // courbe lissée (Catmull-Rom -> Bézier)
    QPainterPath line;
    line.moveTo(pts[0]);
    for (int i = 0; i < n - 1; ++i) {
        const QPointF p0 = pts[std::max(i - 1, 0)];
        const QPointF p1 = pts[i];
        const QPointF p2 = pts[i + 1];
        const QPointF p3 = pts[std::min(i + 2, n - 1)];
        line.cubicTo(p1 + (p2 - p0) / 6.0, p2 - (p3 - p1) / 6.0, p2);
    }

    QPainterPath area = line;
    area.lineTo(pts.last().x(), plot.bottom());
    area.lineTo(pts.first().x(), plot.bottom());
    area.closeSubpath();

    QLinearGradient g(0, plot.top(), 0, plot.bottom());
    g.setColorAt(0.0, QColor(kGold.red(), kGold.green(), kGold.blue(), 95));
    g.setColorAt(1.0, QColor(kGold.red(), kGold.green(), kGold.blue(), 0));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawPath(area);

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(kGold, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(line);

    p.setBrush(kGold);
    p.setPen(Qt::NoPen);
    p.drawEllipse(pts.last(), 4.5, 4.5);

    p.setPen(kMuted);
    for (int i = 0; i < n && i < m_labels.size(); ++i)
        p.drawText(QRectF(pts[i].x() - 25, plot.bottom() + 6, 50, 18), Qt::AlignCenter, m_labels[i]);
}

// ============================================================ StatusChart
StatusChart::StatusChart(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(190);
}

void StatusChart::setData(const QStringList &labels, const QVector<double> &values, const QVector<QColor> &colors)
{
    m_labels = labels;
    m_values = values;
    m_colors = colors;
    update();
}

void StatusChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QFont f = font();
    f.setPointSize(9);
    p.setFont(f);

    const int n = m_values.size();
    if (n == 0) return;
    double maxV = 1;
    for (double v : m_values) maxV = std::max(maxV, v);

    const QRectF plot(16, 22, width() - 32, height() - 22 - 28);
    const double slot = plot.width() / n;
    const double bw = std::min(64.0, slot * 0.5);

    p.setPen(QPen(QColor(255, 255, 255, 20), 1));
    p.drawLine(QPointF(plot.left(), plot.bottom()), QPointF(plot.right(), plot.bottom()));

    for (int i = 0; i < n; ++i) {
        const double cx = plot.left() + slot * (i + 0.5);
        const double h = m_values[i] / maxV * plot.height();
        const QRectF bar(cx - bw / 2, plot.bottom() - h, bw, h);
        const QColor c = i < m_colors.size() ? m_colors[i] : kGold;

        QLinearGradient g(0, bar.top(), 0, bar.bottom());
        g.setColorAt(0, c);
        g.setColorAt(1, QColor(c.red(), c.green(), c.blue(), 90));
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        if (h > 0) p.drawRoundedRect(bar, 5, 5);

        p.setPen(QColor(0xf3, 0xf0, 0xea));
        p.drawText(QRectF(cx - slot / 2, bar.top() - 20, slot, 18), Qt::AlignCenter, QString::number(qRound(m_values[i])));
        p.setPen(kMuted);
        p.drawText(QRectF(cx - slot / 2, plot.bottom() + 6, slot, 18), Qt::AlignCenter,
                   i < m_labels.size() ? m_labels[i] : QString());
    }
}

// ============================================================ OrderWindow
OrderWindow::OrderWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("AUREA — Gestion des commandes"));
    resize(1260, 800);

    // Le style est propre à ce module (il ne modifie ni celui des Employés ni celui des Clients).
    QFile theme(QStringLiteral(":/orders/theme.qss"));
    if (theme.open(QIODevice::ReadOnly)) setStyleSheet(QString::fromUtf8(theme.readAll()));

    m_ready = openDatabase();
    buildUi();

    // Recherche globale : filtre la liste des commandes et ouvre cette page.
    connect(m_globalSearch, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_cmdFilter->setText(text);
        if (!text.trimmed().isEmpty()) setPage(1);
    });

    refreshAll();

    m_clock = new QTimer(this);
    connect(m_clock, &QTimer::timeout, this, &OrderWindow::updateClock);
    m_clock->start(20 * 1000);
    updateClock();
    setPage(0);
}

void OrderWindow::setSidebarVisible(bool visible)
{
    m_sidebar->setVisible(visible);
}

void OrderWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // Les clients (et leur statut VIP) peuvent avoir changé dans le module Clients.
    if (m_cmdTable) refreshAll();
}

// ------------------------------------------------------------ Base de données
// Le module partage la base du module Clients (connexion Qt par défaut) :
// les clients et leur statut VIP viennent donc directement de la gestion des clients.
bool OrderWindow::openDatabase()
{
    m_db = QSqlDatabase::database(QSqlDatabase::defaultConnection, false);
    if (!m_db.isValid() || !m_db.isOpen()) return false;
    m_dbPath = m_db.databaseName();
    createTables();
    seedIfEmpty();
    return true;
}

void OrderWindow::createTables()
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS matieres ("
                          "id INTEGER PRIMARY KEY AUTOINCREMENT, nom TEXT NOT NULL, type TEXT, couleur TEXT,"
                          "quantite REAL DEFAULT 0, seuil_min REAL DEFAULT 0, prix REAL DEFAULT 0,"
                          "fournisseur TEXT, id_lot TEXT)"));
    // « commandes » existe déjà dans la base Clients (historique d'achats de la fidélité) :
    // les commandes d'atelier ont donc leur propre table.
    q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS commandes_atelier ("
                          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "date_commande TEXT NOT NULL, client_id INTEGER NOT NULL, modele TEXT NOT NULL,"
                          "montant REAL DEFAULT 0, acompte REAL DEFAULT 0,"
                          "date_livraison TEXT NOT NULL, statut TEXT NOT NULL DEFAULT 'En attente')"));
    q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS atelier_meta (cle TEXT PRIMARY KEY, valeur TEXT)"));
}

void OrderWindow::seedIfEmpty()
{
    QSqlQuery flag(m_db);
    flag.exec(QStringLiteral("SELECT valeur FROM atelier_meta WHERE cle='commandes_exemples'"));
    if (flag.next()) return;   // exemples déjà chargés une fois : on ne les recharge pas

    QVector<int> clientIds;
    QSqlQuery cq(m_db);
    cq.exec(QStringLiteral("SELECT id FROM clients ORDER BY id"));
    while (cq.next()) clientIds << cq.value(0).toInt();
    if (clientIds.isEmpty()) return;   // les commandes d'exemple ont besoin de clients

    m_db.transaction();

    QSqlQuery mcnt(m_db);
    mcnt.exec(QStringLiteral("SELECT COUNT(*) FROM matieres"));
    if (mcnt.next() && mcnt.value(0).toInt() == 0) {
        struct M { const char *nom, *type, *couleur; double qte, seuil, prix; const char *four, *lot; };
        const M ms[] = {
            {"Soie duchesse", "Tissu", "Ivoire", 42, 15, 68, "Soieries de Lyon", "L-2026-014"},
            {"Satin de soie", "Tissu", "Noir", 8, 12, 52, "Soieries de Lyon", "L-2026-016"},
            {"Dentelle de Calais", "Dentelle", "Blanc cassé", 25, 10, 120, "Maison Solstiss", "L-2026-021"},
            {"Organza", "Tissu", "Champagne", 30, 10, 38, "Soieries de Lyon", "L-2026-009"},
            {"Laine bouclée", "Tissu", "Bordeaux", 18, 8, 55, "Lainières d'Italie", "L-2026-017"},
            {"Doublure viscose", "Tissu", "Beige", 60, 20, 12, "Textiles du Cap Bon", "L-2026-005"},
            {"Fil d'or", "Fil", "Or", 6, 8, 14.5, "Passementerie Royale", "L-2026-030"},
            {"Perles de nacre (sachet)", "Perles", "Nacre", 40, 10, 22, "Perlerie d'Orient", "L-2026-027"},
            {"Boutons nacre (lot de 10)", "Mercerie", "Nacre", 15, 6, 12, "Passementerie Royale", "L-2026-031"},
            {"Fermeture invisible", "Mercerie", "Noir", 40, 50, 2.8, "Mercerie Centrale", "L-2026-002"}};
        QSqlQuery mi(m_db);
        mi.prepare(QStringLiteral("INSERT INTO matieres(nom,type,couleur,quantite,seuil_min,prix,fournisseur,id_lot) "
                                  "VALUES(?,?,?,?,?,?,?,?)"));
        for (const M &m : ms) {
            mi.addBindValue(QString::fromUtf8(m.nom));
            mi.addBindValue(QString::fromUtf8(m.type));
            mi.addBindValue(QString::fromUtf8(m.couleur));
            mi.addBindValue(m.qte);
            mi.addBindValue(m.seuil);
            mi.addBindValue(m.prix);
            mi.addBindValue(QString::fromUtf8(m.four));
            mi.addBindValue(QString::fromUtf8(m.lot));
            mi.exec();
        }
    }

    QSqlQuery ocnt(m_db);
    ocnt.exec(QStringLiteral("SELECT COUNT(*) FROM commandes_atelier"));
    if (ocnt.next() && ocnt.value(0).toInt() == 0) {
        // commandes de démonstration (dates relatives à aujourd'hui) ; le client « n » est le n-ième client existant
        struct O { int off; int client; const char *modele; double montant, acompte; int livr; const char *statut; };
        const O os[] = {
            {-170, 1, "Robe de mariée sirène", 5200, 1500, -120, "Livrée"},
            {-140, 3, "Caftan brodé main", 3400, 1000, -95, "Livrée"},
            {-118, 2, "Tailleur haute couture", 2800, 800, -80, "Livrée"},
            {-95, 5, "Robe de soirée drapée", 2100, 600, -60, "Livrée"},
            {-70, 4, "Manteau en laine bouclée", 1900, 500, -40, "Livrée"},
            {-55, 6, "Ensemble cérémonie", 2600, 800, -25, "Livrée"},
            {-38, 1, "Robe fourreau satin", 1750, 500, -10, "Livrée"},
            {-30, 7, "Jupe plissée soie", 980, 300, -3, "Terminée"},
            {-22, 2, "Robe de mariée princesse", 6100, 2000, -4, "En cours"},
            {-16, 8, "Caftan brodé main", 3900, 1200, 1, "En cours"},
            {-12, 3, "Tailleur haute couture", 3100, 900, 2, "En attente"},
            {-9, 5, "Robe de soirée drapée", 2450, 700, 6, "En attente"},
            {-6, 4, "Manteau en laine bouclée", 2200, 600, 12, "En cours"},
            {-2, 6, "Ensemble cérémonie", 2750, 800, 25, "En attente"}};
        QSqlQuery oi(m_db);
        oi.prepare(QStringLiteral("INSERT INTO commandes_atelier(date_commande, client_id, modele, montant, acompte, date_livraison, statut) "
                                  "VALUES(?,?,?,?,?,?,?)"));
        const QDate today = QDate::currentDate();
        for (const O &o : os) {
            oi.addBindValue(today.addDays(o.off).toString(Qt::ISODate));
            oi.addBindValue(clientIds[(o.client - 1) % clientIds.size()]);
            oi.addBindValue(QString::fromUtf8(o.modele));
            oi.addBindValue(o.montant);
            oi.addBindValue(o.acompte);
            oi.addBindValue(today.addDays(o.livr).toString(Qt::ISODate));
            oi.addBindValue(QString::fromUtf8(o.statut));
            oi.exec();
        }
    }

    QSqlQuery mark(m_db);
    mark.exec(QStringLiteral("INSERT INTO atelier_meta(cle, valeur) VALUES('commandes_exemples', '1')"));
    m_db.commit();
}

bool OrderWindow::insertCommande(const AtelierOrder &c)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO commandes_atelier(date_commande, client_id, modele, montant, acompte, date_livraison, statut) "
                             "VALUES(?,?,?,?,?,?,?)"));
    q.addBindValue(c.date.toString(Qt::ISODate));
    q.addBindValue(c.clientId);
    q.addBindValue(c.modele);
    q.addBindValue(c.montant);
    q.addBindValue(c.acompte);
    q.addBindValue(c.livraison.toString(Qt::ISODate));
    q.addBindValue(c.statut);
    if (!q.exec()) {
        sqlError(this, q);
        return false;
    }
    return true;
}

QVector<AtelierClient> OrderWindow::loadClients() const
{
    QVector<AtelierClient> list;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT id, nom, categorie FROM clients ORDER BY nom"));
    while (q.next()) {
        AtelierClient c;
        c.id = q.value(0).toInt();
        c.nom = q.value(1).toString();
        c.vip = q.value(2).toString().compare(QLatin1String("VIP"), Qt::CaseInsensitive) == 0;
        list << c;
    }
    return list;
}

QVector<AtelierOrder> OrderWindow::loadCommandes() const
{
    QVector<AtelierOrder> list;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT id, date_commande, client_id, modele, montant, acompte, date_livraison, statut "
                          "FROM commandes_atelier ORDER BY id"));
    while (q.next()) {
        AtelierOrder c;
        c.id = q.value(0).toInt();
        c.date = QDate::fromString(q.value(1).toString(), Qt::ISODate);
        c.clientId = q.value(2).toInt();
        c.modele = q.value(3).toString();
        c.montant = q.value(4).toDouble();
        c.acompte = q.value(5).toDouble();
        c.livraison = QDate::fromString(q.value(6).toString(), Qt::ISODate);
        c.statut = q.value(7).toString();
        list << c;
    }
    return list;
}

QVector<AtelierMaterial> OrderWindow::loadMatieres() const
{
    QVector<AtelierMaterial> list;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT id, nom, type, couleur, quantite, seuil_min, prix, fournisseur, id_lot "
                          "FROM matieres ORDER BY nom"));
    while (q.next()) {
        AtelierMaterial m;
        m.id = q.value(0).toInt();
        m.nom = q.value(1).toString();
        m.type = q.value(2).toString();
        m.couleur = q.value(3).toString();
        m.quantite = q.value(4).toDouble();
        m.seuil = q.value(5).toDouble();
        m.prix = q.value(6).toDouble();
        m.fournisseur = q.value(7).toString();
        m.lot = q.value(8).toString();
        list << m;
    }
    return list;
}

bool OrderWindow::isVip(int clientId) const
{
    for (const AtelierClient &c : m_clients)
        if (c.id == clientId) return c.vip;
    return false;
}

QString OrderWindow::clientLabel(int clientId) const
{
    for (const AtelierClient &c : m_clients)
        if (c.id == clientId)
            return QStringLiteral("C%1 · %2").arg(c.id, 3, 10, QChar('0')).arg(c.nom);
    return QStringLiteral("C%1").arg(clientId, 3, 10, QChar('0'));
}

// ------------------------------------------------------------ Interface
void OrderWindow::buildUi()
{
    auto *central = new QWidget;
    central->setObjectName(QStringLiteral("centralwidget"));
    setCentralWidget(central);
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(buildSidebar());

    auto *column = new QWidget;
    column->setObjectName(QStringLiteral("mainColumn"));
    auto *cv = new QVBoxLayout(column);
    cv->setContentsMargins(0, 0, 0, 0);
    cv->setSpacing(0);
    cv->addWidget(buildTopbar());

    m_pages = new QStackedWidget;
    m_pages->setObjectName(QStringLiteral("orderPages"));
    m_pages->addWidget(buildDashboardPage());
    m_pages->addWidget(buildCommandesPage());
    m_pages->addWidget(buildDevisPage());
    cv->addWidget(m_pages, 1);

    m_footer = new QLabel(m_ready ? QStringLiteral("Gestion des commandes · sauvegarde SQLite immédiate")
                                  : QStringLiteral("Gestion des commandes · base de données indisponible"));
    m_footer->setObjectName(QStringLiteral("footerNote"));
    cv->addWidget(m_footer);

    root->addWidget(column, 1);
}

QWidget *OrderWindow::buildSidebar()
{
    m_sidebar = new QFrame;
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setFixedWidth(240);
    auto *v = new QVBoxLayout(m_sidebar);
    v->setContentsMargins(16, 22, 16, 18);
    v->setSpacing(6);

    auto *logo = new QLabel;
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(QPixmap(QStringLiteral(":/assets/logo_aurea.png")).scaled(70, 70, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    auto *word = new QLabel(QStringLiteral("AUREA"));
    word->setObjectName(QStringLiteral("logoWord"));
    word->setAlignment(Qt::AlignCenter);
    auto *tagline = new QLabel(QStringLiteral("ATELIER HAUTE COUTURE"));
    tagline->setObjectName(QStringLiteral("logoTagline"));
    tagline->setAlignment(Qt::AlignCenter);
    v->addWidget(logo);
    v->addWidget(word);
    v->addWidget(tagline);
    v->addSpacing(26);

    const struct { QString text; Icons::Kind icon; } defs[] = {
        {QStringLiteral("Tableau de bord"), Icons::Grid},
        {QStringLiteral("Commandes"), Icons::Assign},
        {QStringLiteral("Devis"), Icons::Document},
    };
    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    for (int i = 0; i < 3; ++i) {
        auto *b = new QPushButton(QStringLiteral("  ") + defs[i].text, m_sidebar);
        b->setProperty("nav", true);
        b->setCheckable(true);
        b->setIcon(Icons::nav(defs[i].icon));
        b->setIconSize(QSize(20, 20));
        b->setCursor(Qt::PointingHandCursor);
        group->addButton(b, i);
        m_navButtons << b;
        v->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, i] { setPage(i); });
        auto *shortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(i + 1)), this);
        connect(shortcut, &QShortcut::activated, this, [this, i] { setPage(i); });
    }
    v->addStretch(1);

    m_help = new QPushButton(QStringLiteral("  Aide"), m_sidebar);
    m_help->setProperty("nav", true);
    m_help->setIcon(Icons::nav(Icons::Help));
    m_help->setIconSize(QSize(20, 20));
    m_help->setCursor(Qt::PointingHandCursor);
    connect(m_help, &QPushButton::clicked, this, &OrderWindow::showHelp);
    v->addWidget(m_help);
    return m_sidebar;
}

QWidget *OrderWindow::buildTopbar()
{
    auto *bar = new QFrame;
    bar->setObjectName(QStringLiteral("topbar"));
    bar->setFixedHeight(76);
    auto *h = new QHBoxLayout(bar);
    h->setContentsMargins(26, 0, 26, 0);
    h->setSpacing(16);

    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    m_pageTitle = new QLabel;
    m_pageTitle->setObjectName(QStringLiteral("pageTitle"));
    m_subtitle = new QLabel;
    m_subtitle->setObjectName(QStringLiteral("subtitle"));
    titles->addStretch(1);
    titles->addWidget(m_pageTitle);
    titles->addWidget(m_subtitle);
    titles->addStretch(1);
    h->addLayout(titles);
    h->addStretch(1);

    m_globalSearch = new QLineEdit;
    m_globalSearch->setObjectName(QStringLiteral("globalSearch"));
    m_globalSearch->setPlaceholderText(QStringLiteral("Rechercher une commande..."));
    m_globalSearch->setMinimumWidth(320);
    m_globalSearch->addAction(QIcon(Icons::pixmap(Icons::Search, kMuted, 16)), QLineEdit::LeadingPosition);
    h->addWidget(m_globalSearch);
    h->addStretch(1);

    m_dateLabel = new QLabel;
    m_dateLabel->setObjectName(QStringLiteral("dateLabel"));
    h->addWidget(m_dateLabel);
    return bar;
}

QWidget *OrderWindow::buildDashboardPage()
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("pageContent"));
    scroll->setWidget(content);

    auto *v = new QVBoxLayout(content);
    v->setContentsMargins(26, 22, 26, 12);
    v->setSpacing(16);

    // --- cartes de statistiques
    auto *stats = new QHBoxLayout;
    stats->setSpacing(14);
    stats->addWidget(makeStatCard(QStringLiteral("Commandes"), Icons::Assign, &m_statTotal, &m_statTotalSub), 1);
    stats->addWidget(makeStatCard(QStringLiteral("En cours"), Icons::Clock, &m_statEnCours, &m_statEnCoursSub), 1);
    stats->addWidget(makeStatCard(QStringLiteral("Livrées"), Icons::Check, &m_statLivrees, &m_statLivreesSub), 1);
    stats->addWidget(makeStatCard(QStringLiteral("Chiffre d'affaires"), Icons::Chart, &m_statCA, &m_statCASub), 1);
    m_alertBtn = new QPushButton;
    m_alertBtn->setObjectName(QStringLiteral("alertButton"));
    m_alertBtn->setIcon(Icons::pixmap(Icons::Alert, QColor("#2a2417"), 20));
    m_alertBtn->setIconSize(QSize(20, 20));
    m_alertBtn->setCursor(Qt::PointingHandCursor);
    m_alertBtn->setMinimumWidth(230);
    connect(m_alertBtn, &QPushButton::clicked, this, [this]() {
        // ouvre la liste, retards en tête
        m_cmdFilter->clear();
        m_cmdSortDir->setProperty("desc", true);
        m_cmdSortDir->setText(QStringLiteral("↓ Décroissant"));
        const int idx = m_cmdSort->findData(8);
        if (idx >= 0) m_cmdSort->setCurrentIndex(idx);
        applyCommandeSort();
        setPage(1);
    });
    stats->addWidget(m_alertBtn, 1);
    v->addLayout(stats);

    // --- deux colonnes
    auto *cols = new QHBoxLayout;
    cols->setSpacing(16);
    auto *left = new QVBoxLayout;
    left->setSpacing(16);
    auto *right = new QVBoxLayout;
    right->setSpacing(16);

    QLayout *body = nullptr;

    left->addWidget(makeCard(QStringLiteral("Chiffre d'affaires"), &body, makePill(QStringLiteral("6 derniers mois"))), 3);
    m_caChart = new RevenueChart;
    body->addWidget(m_caChart);

    left->addWidget(makeCard(QStringLiteral("Commandes par statut"), &body, makePill(QStringLiteral("Toutes les commandes"))), 2);
    m_statutChart = new StatusChart;
    m_statutChart->setMinimumHeight(200);
    body->addWidget(m_statutChart);

    right->addWidget(makeCard(QStringLiteral("Livraisons à surveiller"), &body, makePill(QStringLiteral("Plus urgentes en premier"))), 3);
    m_dashRetards = new QTableWidget;
    setupTable(m_dashRetards, {QStringLiteral("Commande"), QStringLiteral("Client"), QStringLiteral("Livraison"), QStringLiteral("Suivi")});
    m_dashRetards->setItemDelegate(new TintDelegate(m_dashRetards));
    m_dashRetards->setMinimumHeight(300);
    body->addWidget(m_dashRetards);

    right->addWidget(makeCard(QStringLiteral("Commandes récentes"), &body), 2);
    m_dashRecent = new QTableWidget;
    setupTable(m_dashRecent, {QStringLiteral("Date"), QStringLiteral("Client"), QStringLiteral("Modèle"), QStringLiteral("Montant")});
    m_dashRecent->setMinimumHeight(260);
    body->addWidget(m_dashRecent);

    cols->addLayout(left, 1);
    cols->addLayout(right, 1);
    v->addLayout(cols, 1);
    return scroll;
}

QWidget *OrderWindow::buildCommandesPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("pageContent"));
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(26, 22, 26, 12);
    v->setSpacing(14);

    auto *bar = new QHBoxLayout;
    bar->setSpacing(8);
    m_cmdFilter = new QLineEdit;
    m_cmdFilter->setPlaceholderText(QStringLiteral("Rechercher dans la liste..."));
    m_cmdFilter->setClearButtonEnabled(true);
    m_cmdFilter->setMinimumWidth(200);
    m_cmdFilter->setMaximumWidth(340);
    m_cmdFilter->addAction(QIcon(Icons::pixmap(Icons::Search, kMuted, 16)), QLineEdit::LeadingPosition);

    m_cmdSort = new QComboBox;
    m_cmdSort->addItem(QStringLiteral("Trier : commande"), 1);
    m_cmdSort->addItem(QStringLiteral("Trier : livraison"), 6);
    m_cmdSort->addItem(QStringLiteral("Trier : montant"), 4);
    m_cmdSort->addItem(QStringLiteral("Trier : statut"), 7);
    m_cmdSort->addItem(QStringLiteral("Trier : client"), 2);
    m_cmdSort->addItem(QStringLiteral("Trier : suivi"), 8);
    m_cmdSort->setCurrentIndex(1);
    connect(m_cmdSort, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OrderWindow::applyCommandeSort);

    m_cmdSortDir = new QPushButton(QStringLiteral("↑ Croissant"));
    m_cmdSortDir->setProperty("desc", false);
    connect(m_cmdSortDir, &QPushButton::clicked, this, [this]() {
        const bool desc = !m_cmdSortDir->property("desc").toBool();
        m_cmdSortDir->setProperty("desc", desc);
        m_cmdSortDir->setText(desc ? QStringLiteral("↓ Décroissant") : QStringLiteral("↑ Croissant"));
        applyCommandeSort();
    });

    auto *add = new QPushButton(QStringLiteral("+  Ajouter"));
    add->setProperty("primary", true);
    auto *edit = new QPushButton(QStringLiteral("Modifier"));
    auto *del = new QPushButton(QStringLiteral("Supprimer"));
    del->setProperty("danger", true);
    auto *pdf = new QPushButton(QStringLiteral("Exporter en PDF"));
    for (QPushButton *b : {add, edit, del, pdf, m_cmdSortDir}) b->setCursor(Qt::PointingHandCursor);
    connect(add, &QPushButton::clicked, this, &OrderWindow::addCommande);
    connect(edit, &QPushButton::clicked, this, &OrderWindow::editCommande);
    connect(del, &QPushButton::clicked, this, &OrderWindow::deleteCommande);
    connect(pdf, &QPushButton::clicked, this, &OrderWindow::exportCommandesPdf);

    bar->addWidget(m_cmdFilter, 1);
    bar->addWidget(m_cmdSort);
    bar->addWidget(m_cmdSortDir);
    bar->addStretch(1);
    bar->addWidget(pdf);
    bar->addWidget(add);
    bar->addWidget(edit);
    bar->addWidget(del);
    v->addLayout(bar);

    // légende du suivi des retards
    auto *legend = new QLabel(QStringLiteral("<span style='color:%1'>● À temps</span> &nbsp;&nbsp; "
                                             "<span style='color:%2'>● À risque</span> &nbsp;&nbsp; "
                                             "<span style='color:%3'>● En retard / critique</span>")
                                  .arg(kVert.name(), kOrange.name(), kRouge.name()));
    legend->setProperty("muted", true);
    v->addWidget(legend);

    QLayout *body = nullptr;
    v->addWidget(makeCard(QStringLiteral("Liste des commandes"), &body), 1);
    m_cmdTable = new QTableWidget;
    setupTable(m_cmdTable, {QStringLiteral("ID"), QStringLiteral("Date"), QStringLiteral("Client"), QStringLiteral("Modèle"),
                            QStringLiteral("Montant"), QStringLiteral("Acompte"), QStringLiteral("Livraison"),
                            QStringLiteral("Statut"), QStringLiteral("Suivi")});
    for (int i = 0; i < m_cmdTable->columnCount(); ++i)
        m_cmdTable->horizontalHeader()->setSectionResizeMode(i, (i == 3) ? QHeaderView::Stretch : QHeaderView::ResizeToContents);
    m_cmdTable->setItemDelegate(new TintDelegate(m_cmdTable));
    m_cmdTable->horizontalHeader()->setSortIndicator(6, Qt::AscendingOrder);
    m_cmdTable->setSortingEnabled(true);
    connect(m_cmdTable, &QTableWidget::cellDoubleClicked, this, [this]() { editCommande(); });
    body->addWidget(m_cmdTable);

    connect(m_cmdFilter, &QLineEdit::textChanged, this, &OrderWindow::applyCommandeFilter);
    return page;
}

QWidget *OrderWindow::buildDevisPage()
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("pageContent"));
    scroll->setWidget(page);

    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(26, 22, 26, 12);
    v->setSpacing(16);

    auto *cols = new QHBoxLayout;
    cols->setSpacing(16);
    auto *left = new QVBoxLayout;
    left->setSpacing(16);
    auto *right = new QVBoxLayout;
    right->setSpacing(16);

    QLayout *body = nullptr;

    // --- paramètres
    left->addWidget(makeCard(QStringLiteral("Paramètres"), &body));
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(12);
    m_dvClient = new QComboBox;
    m_dvVipLabel = new QLabel;
    m_dvVipLabel->setObjectName(QStringLiteral("vipBadge"));
    m_dvModele = new QLineEdit;
    m_dvModele->setPlaceholderText(QStringLiteral("Ex. Robe de soirée drapée"));
    m_dvHeures = new QDoubleSpinBox;
    m_dvHeures->setRange(0, 1000);
    m_dvHeures->setDecimals(1);
    m_dvHeures->setSingleStep(0.5);
    m_dvHeures->setValue(20);
    m_dvHeures->setSuffix(QStringLiteral(" h"));
    m_dvTaux = new QDoubleSpinBox;
    m_dvTaux->setRange(0, 10000);
    m_dvTaux->setDecimals(2);
    m_dvTaux->setValue(kTauxHoraire);
    m_dvTaux->setSuffix(QStringLiteral(" DT / h"));
    m_dvRemise = new QDoubleSpinBox;
    m_dvRemise->setRange(0, 100);
    m_dvRemise->setDecimals(1);
    m_dvRemise->setSuffix(QStringLiteral(" %"));

    grid->addWidget(fieldLabel(QStringLiteral("Client")), 0, 0);
    grid->addWidget(m_dvClient, 0, 1);
    grid->addWidget(m_dvVipLabel, 0, 2);
    grid->addWidget(fieldLabel(QStringLiteral("Modèle")), 1, 0);
    grid->addWidget(m_dvModele, 1, 1, 1, 2);
    grid->addWidget(fieldLabel(QStringLiteral("Heures de travail")), 2, 0);
    grid->addWidget(m_dvHeures, 2, 1, 1, 2);
    grid->addWidget(fieldLabel(QStringLiteral("Taux horaire")), 3, 0);
    grid->addWidget(m_dvTaux, 3, 1, 1, 2);
    grid->addWidget(fieldLabel(QStringLiteral("Remise VIP")), 4, 0);
    grid->addWidget(m_dvRemise, 4, 1, 1, 2);
    grid->setColumnStretch(1, 1);
    static_cast<QVBoxLayout *>(body)->addLayout(grid);

    // --- matières
    left->addWidget(makeCard(QStringLiteral("Matières utilisées"), &body), 1);
    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    m_dvMatiere = new QComboBox;
    m_dvQte = new QDoubleSpinBox;
    m_dvQte->setRange(0.1, 10000);
    m_dvQte->setDecimals(1);
    m_dvQte->setValue(2);
    auto *addLine = new QPushButton(QStringLiteral("Ajouter"));
    addLine->setProperty("primary", true);
    auto *delLine = new QPushButton(QStringLiteral("Retirer"));
    delLine->setProperty("danger", true);
    addLine->setCursor(Qt::PointingHandCursor);
    delLine->setCursor(Qt::PointingHandCursor);
    connect(addLine, &QPushButton::clicked, this, &OrderWindow::addDevisLine);
    connect(delLine, &QPushButton::clicked, this, &OrderWindow::removeDevisLine);
    row->addWidget(m_dvMatiere, 1);
    row->addWidget(fieldLabel(QStringLiteral("Qté")));
    row->addWidget(m_dvQte);
    row->addWidget(addLine);
    row->addWidget(delLine);
    static_cast<QVBoxLayout *>(body)->addLayout(row);
    m_dvTable = new QTableWidget;
    setupTable(m_dvTable, {QStringLiteral("Matière"), QStringLiteral("Quantité"), QStringLiteral("Prix unitaire"), QStringLiteral("Total")});
    m_dvTable->setMinimumHeight(220);
    body->addWidget(m_dvTable);

    // --- récapitulatif
    right->addWidget(makeCard(QStringLiteral("Récapitulatif"), &body));
    auto *sum = new QGridLayout;
    sum->setHorizontalSpacing(16);
    sum->setVerticalSpacing(10);
    auto addRow = [&](int r, const QString &label, QLabel **val) {
        auto *l = fieldLabel(label);
        *val = new QLabel(QStringLiteral("0"));
        (*val)->setObjectName(QStringLiteral("sumValue"));
        (*val)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        sum->addWidget(l, r, 0);
        sum->addWidget(*val, r, 1);
    };
    addRow(0, QStringLiteral("Matières"), &m_dvMat);
    addRow(1, QStringLiteral("Main-d'œuvre"), &m_dvMO);
    addRow(2, QStringLiteral("Sous-total"), &m_dvSub);
    addRow(3, QStringLiteral("Remise VIP"), &m_dvRem);
    auto *rule = new QFrame;
    rule->setProperty("rule", true);
    rule->setFixedHeight(1);
    sum->addWidget(rule, 4, 0, 1, 2);
    auto *tl = new QLabel(QStringLiteral("Total net"));
    tl->setObjectName(QStringLiteral("totalLabel"));
    m_dvTotal = new QLabel(QStringLiteral("0"));
    m_dvTotal->setObjectName(QStringLiteral("totalValue"));
    m_dvTotal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    sum->addWidget(tl, 5, 0);
    sum->addWidget(m_dvTotal, 5, 1);
    sum->setColumnStretch(0, 1);
    static_cast<QVBoxLayout *>(body)->addLayout(sum);

    auto *pdf = new QPushButton(QStringLiteral("Exporter le devis en PDF"));
    pdf->setProperty("primary", true);
    auto *create = new QPushButton(QStringLiteral("Créer la commande"));
    pdf->setCursor(Qt::PointingHandCursor);
    create->setCursor(Qt::PointingHandCursor);
    connect(pdf, &QPushButton::clicked, this, &OrderWindow::exportDevisPdf);
    connect(create, &QPushButton::clicked, this, &OrderWindow::createCommandeFromDevis);
    body->addWidget(pdf);
    body->addWidget(create);
    right->addStretch(1);

    cols->addLayout(left, 3);
    cols->addLayout(right, 2);
    v->addLayout(cols, 1);

    connect(m_dvClient, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OrderWindow::onDevisClientChanged);
    connect(m_dvHeures, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OrderWindow::updateDevis);
    connect(m_dvTaux, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OrderWindow::updateDevis);
    connect(m_dvRemise, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OrderWindow::updateDevis);
    return scroll;
}

// ------------------------------------------------------------ Rafraîchissement
void OrderWindow::refreshAll()
{
    m_clients = loadClients();
    reloadCombos();
    refreshCommandes();
    refreshDashboard();
    updateDevis();
}

void OrderWindow::reloadCombos()
{
    // client du devis
    const int oldClient = m_dvClient->currentData().toInt();
    m_dvClient->blockSignals(true);
    m_dvClient->clear();
    for (const AtelierClient &c : m_clients)
        m_dvClient->addItem(QStringLiteral("C%1 · %2").arg(c.id, 3, 10, QChar('0')).arg(c.nom), c.id);
    const int ci = m_dvClient->findData(oldClient);
    if (ci >= 0) m_dvClient->setCurrentIndex(ci);
    m_dvClient->blockSignals(false);
    // première fois, ou statut VIP du client modifié dans le module Clients : (ré)applique la remise
    if (ci < 0 || int(isVip(m_dvClient->currentData().toInt())) != m_dvLastVip) onDevisClientChanged();

    // matières du devis
    const int oldMat = m_dvMatiere->currentData().toInt();
    m_dvMatiere->clear();
    for (const AtelierMaterial &m : loadMatieres())
        m_dvMatiere->addItem(QStringLiteral("%1 — %2").arg(m.nom, fmtMoney(m.prix)), m.id);
    const int mi = m_dvMatiere->findData(oldMat);
    if (mi >= 0) m_dvMatiere->setCurrentIndex(mi);
}

void OrderWindow::refreshDashboard()
{
    const QVector<AtelierOrder> cmds = loadCommandes();

    int enCours = 0, livrees = 0, retards = 0;
    double ca = 0, acomptes = 0;
    QVector<int> parStatut(kStatuts.size(), 0);
    for (const AtelierOrder &c : cmds) {
        ca += c.montant;
        acomptes += c.acompte;
        if (c.statut == QLatin1String("En cours")) ++enCours;
        if (c.statut == QStringLiteral("Livrée")) ++livrees;
        const int si = kStatuts.indexOf(c.statut);
        if (si >= 0) ++parStatut[si];
        if (suiviFor(c).niveau == 2) ++retards;
    }

    const QLocale fr(QLocale::French);
    m_statTotal->setText(QString::number(cmds.size()));
    m_statTotalSub->setText(QStringLiteral("au total"));
    m_statEnCours->setText(QString::number(enCours));
    m_statEnCoursSub->setText(QStringLiteral("sur %1").arg(cmds.size()));
    m_statLivrees->setText(QString::number(livrees));
    m_statLivreesSub->setText(QStringLiteral("sur %1").arg(cmds.size()));
    m_statCA->setText(fr.toString(qRound(ca)));
    m_statCASub->setText(QStringLiteral("DT · acomptes %1").arg(fr.toString(qRound(acomptes))));

    m_alertBtn->setText(retards > 0 ? QStringLiteral("Retards : %1   ›").arg(retards)
                                    : QStringLiteral("Aucun retard   ›"));

    // chiffre d'affaires sur 6 mois
    QStringList labels;
    QVector<double> values;
    const QDate firstOfMonth(QDate::currentDate().year(), QDate::currentDate().month(), 1);
    for (int i = 5; i >= 0; --i) {
        const QDate m = firstOfMonth.addMonths(-i);
        labels << fr.toString(m, QStringLiteral("MMM")).remove('.');
        double sum = 0;
        for (const AtelierOrder &c : cmds)
            if (c.date.year() == m.year() && c.date.month() == m.month()) sum += c.montant;
        values << sum;
    }
    m_caChart->setData(labels, values, QStringLiteral(" DT"));

    // commandes par statut
    QVector<double> sv;
    for (int n : parStatut) sv << n;
    m_statutChart->setData(kStatuts, sv, {kMuted, kGold, kVert, kBleu});

    // livraisons à surveiller (non livrées, les plus urgentes d'abord)
    QVector<AtelierOrder> open;
    for (const AtelierOrder &c : cmds)
        if (c.statut != QStringLiteral("Livrée")) open << c;
    std::sort(open.begin(), open.end(), [](const AtelierOrder &a, const AtelierOrder &b) { return a.livraison < b.livraison; });
    m_dashRetards->setRowCount(0);
    for (int i = 0; i < open.size() && i < 8; ++i) {
        const AtelierOrder &c = open[i];
        const Suivi s = suiviFor(c);
        const int r = m_dashRetards->rowCount();
        m_dashRetards->insertRow(r);
        setCell(m_dashRetards, r, 0, cmdCode(c.id));
        setCell(m_dashRetards, r, 1, clientLabel(c.clientId).section(QStringLiteral(" · "), 1));
        setCell(m_dashRetards, r, 2, c.livraison.toString(QStringLiteral("dd/MM/yy")));
        auto *it = setCell(m_dashRetards, r, 3, s.texte);
        it->setForeground(s.couleur);
        tintRow(m_dashRetards, r, s.couleur);
    }

    // commandes récentes
    QVector<AtelierOrder> recent = cmds;
    std::sort(recent.begin(), recent.end(), [](const AtelierOrder &a, const AtelierOrder &b) {
        return a.date != b.date ? a.date > b.date : a.id > b.id;
    });
    m_dashRecent->setRowCount(0);
    for (int i = 0; i < recent.size() && i < 8; ++i) {
        const AtelierOrder &c = recent[i];
        const int r = m_dashRecent->rowCount();
        m_dashRecent->insertRow(r);
        setCell(m_dashRecent, r, 0, c.date.toString(QStringLiteral("dd/MM/yy")));
        setCell(m_dashRecent, r, 1, clientLabel(c.clientId).section(QStringLiteral(" · "), 1));
        setCell(m_dashRecent, r, 2, c.modele);
        setCell(m_dashRecent, r, 3, fmtMoney(c.montant, 0));
    }
}

void OrderWindow::refreshCommandes()
{
    const QVector<AtelierOrder> cmds = loadCommandes();

    m_cmdTable->setSortingEnabled(false);
    m_cmdTable->setRowCount(0);
    for (const AtelierOrder &c : cmds) {
        const Suivi s = suiviFor(c);
        const int r = m_cmdTable->rowCount();
        m_cmdTable->insertRow(r);
        setKeyCell(m_cmdTable, r, 0, cmdCode(c.id), c.id, c.id);
        setKeyCell(m_cmdTable, r, 1, c.date.toString(QStringLiteral("dd/MM/yy")), c.date.toJulianDay());
        setCell(m_cmdTable, r, 2, clientLabel(c.clientId) + (isVip(c.clientId) ? QStringLiteral("  ★") : QString()));
        setCell(m_cmdTable, r, 3, c.modele);
        setKeyCell(m_cmdTable, r, 4, fmtMoneyAuto(c.montant), c.montant);
        setKeyCell(m_cmdTable, r, 5, fmtMoneyAuto(c.acompte), c.acompte);
        setKeyCell(m_cmdTable, r, 6, c.livraison.toString(QStringLiteral("dd/MM/yy")), c.livraison.toJulianDay());
        setKeyCell(m_cmdTable, r, 7, c.statut, kStatuts.indexOf(c.statut));
        auto *it = setKeyCell(m_cmdTable, r, 8, s.texte, s.niveau * 1000 - QDate::currentDate().daysTo(c.livraison));
        it->setForeground(s.couleur);
        QFont f = it->font();
        f.setBold(true);
        it->setFont(f);
        tintRow(m_cmdTable, r, s.couleur);
    }
    m_cmdTable->setSortingEnabled(true);
    applyCommandeFilter();
}

// ------------------------------------------------------------ Slots généraux
void OrderWindow::updateClock()
{
    m_dateLabel->setText(QLocale(QLocale::French).toString(QDateTime::currentDateTime(), QStringLiteral("dddd d MMMM yyyy · HH:mm")));
}

void OrderWindow::setPage(int index)
{
    static const QStringList titles = {QStringLiteral("Tableau de bord"), QStringLiteral("Commandes"), QStringLiteral("Devis automatique")};
    static const QStringList subtitles = {QStringLiteral("Les commandes de votre atelier, au quotidien."),
                                          QStringLiteral("Ajoutez, suivez et exportez les commandes de l'atelier."),
                                          QStringLiteral("Prix calculé à partir des matières, des heures et de la remise VIP.")};
    if (index < 0 || index >= titles.size()) return;
    m_pages->setCurrentIndex(index);
    m_pageTitle->setText(titles[index]);
    m_subtitle->setText(subtitles[index]);
    m_navButtons[index]->setChecked(true);
}

void OrderWindow::showHelp()
{
    QMessageBox::information(this, QStringLiteral("AUREA — Guide"),
                             QStringLiteral("Commandes : ajouter, modifier (double-clic), supprimer, rechercher, trier "
                                            "(menu ou clic sur un en-tête) et exporter la liste en PDF.\n\n"
                                            "Suivi des retards : vert = à temps, orange = à risque, rouge = en retard ou critique "
                                            "(selon le statut et la date de livraison).\n\n"
                                            "Devis : choisissez le client, les matières et les heures ; la remise VIP "
                                            "est appliquée automatiquement aux clients VIP du module Clients. Export PDF possible, "
                                            "puis « Créer la commande » pour l'enregistrer.\n\n"
                                            "Ctrl+1 à Ctrl+3 : changer de page.\n\nBase active : ")
                                 + m_dbPath);
}

// ------------------------------------------------------------ Commandes
void OrderWindow::applyCommandeFilter()
{
    const QString f = m_cmdFilter->text().trimmed();
    for (int r = 0; r < m_cmdTable->rowCount(); ++r) {
        bool match = f.isEmpty();
        for (int c = 0; c < m_cmdTable->columnCount() && !match; ++c)
            if (m_cmdTable->item(r, c) && m_cmdTable->item(r, c)->text().contains(f, Qt::CaseInsensitive)) match = true;
        m_cmdTable->setRowHidden(r, !match);
    }
}

void OrderWindow::applyCommandeSort()
{
    const int col = m_cmdSort->currentData().toInt();
    const Qt::SortOrder order = m_cmdSortDir->property("desc").toBool() ? Qt::DescendingOrder : Qt::AscendingOrder;
    m_cmdTable->sortByColumn(col, order);
}

void OrderWindow::addCommande()
{
    AtelierOrder c;
    if (!m_clients.isEmpty()) c.clientId = m_clients.first().id;
    if (!commandeDialog(this, QStringLiteral("Nouvelle commande"), c, m_clients)) return;
    if (insertCommande(c)) refreshAll();
}

void OrderWindow::editCommande()
{
    const int id = selectedId(m_cmdTable);
    if (id < 0) {
        QMessageBox::information(this, QStringLiteral("Commandes"), QStringLiteral("Sélectionnez d'abord une commande."));
        return;
    }
    AtelierOrder c;
    for (const AtelierOrder &x : loadCommandes())
        if (x.id == id) c = x;
    if (!commandeDialog(this, QStringLiteral("Modifier la commande ") + cmdCode(id), c, m_clients)) return;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE commandes_atelier SET date_commande=?, client_id=?, modele=?, montant=?, acompte=?, "
                             "date_livraison=?, statut=? WHERE id=?"));
    q.addBindValue(c.date.toString(Qt::ISODate));
    q.addBindValue(c.clientId);
    q.addBindValue(c.modele);
    q.addBindValue(c.montant);
    q.addBindValue(c.acompte);
    q.addBindValue(c.livraison.toString(Qt::ISODate));
    q.addBindValue(c.statut);
    q.addBindValue(id);
    if (!q.exec()) {
        sqlError(this, q);
        return;
    }
    refreshAll();
}

void OrderWindow::deleteCommande()
{
    const int id = selectedId(m_cmdTable);
    if (id < 0) {
        QMessageBox::information(this, QStringLiteral("Commandes"), QStringLiteral("Sélectionnez d'abord une commande."));
        return;
    }
    if (!askQuestion(this, QStringLiteral("Supprimer"), QStringLiteral("Supprimer la commande %1 ?").arg(cmdCode(id)),
                     QStringLiteral("Supprimer"), QStringLiteral("Annuler"), true))
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM commandes_atelier WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        sqlError(this, q);
        return;
    }
    refreshAll();
}

void OrderWindow::exportCommandesPdf()
{
    // exporte les lignes visibles, dans l'ordre affiché (donc filtre + tri pris en compte)
    QString html = QString::fromUtf8(kPdfHead);
    html += QStringLiteral("<h2 style=\"color:#8a6d2b; margin-bottom:0;\">Liste des commandes</h2>"
                           "<p style=\"color:#777;\">Édité le %1</p>"
                           "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"5\" style=\"font-size:8.5pt;\">")
                .arg(QDate::currentDate().toString(QStringLiteral("dd/MM/yyyy")));
    html += QString::fromUtf8(kPdfHeaderRow);
    const int cols = m_cmdTable->columnCount();
    for (int c = 0; c < cols; ++c)
        html += QStringLiteral("<th align=\"left\">") + m_cmdTable->horizontalHeaderItem(c)->text().toHtmlEscaped() + QStringLiteral("</th>");
    html += QStringLiteral("</tr>");

    int count = 0;
    double total = 0, acomptes = 0;
    for (int r = 0; r < m_cmdTable->rowCount(); ++r) {
        if (m_cmdTable->isRowHidden(r)) continue;
        ++count;
        total += m_cmdTable->item(r, 4)->data(Qt::UserRole + 1).toDouble();
        acomptes += m_cmdTable->item(r, 5)->data(Qt::UserRole + 1).toDouble();
        html += QStringLiteral("<tr>");
        for (int c = 0; c < cols; ++c) {
            QString cell = m_cmdTable->item(r, c)->text().toHtmlEscaped();
            if (c == 8)
                cell = QStringLiteral("<b><font color=\"%1\">%2</font></b>")
                           .arg(m_cmdTable->item(r, c)->foreground().color().darker(150).name(), cell);
            html += QStringLiteral("<td style=\"border-bottom:1px solid #ddd;\">") + cell + QStringLiteral("</td>");
        }
        html += QStringLiteral("</tr>");
    }
    html += QStringLiteral("</table>");
    html += QStringLiteral("<p><b>%1 commande(s)</b> — Montant total : <b>%2</b> — Acomptes : %3 — Reste à encaisser : %4</p>")
                .arg(count).arg(fmtMoney(total), fmtMoney(acomptes), fmtMoney(total - acomptes));
    html += QStringLiteral("</body></html>");

    exportHtmlToPdf(this, html, QStringLiteral("AUREA_commandes_%1.pdf").arg(QDate::currentDate().toString(QStringLiteral("yyyyMMdd"))), true);
}

// ------------------------------------------------------------ Devis automatique
void OrderWindow::onDevisClientChanged()
{
    const bool vip = isVip(m_dvClient->currentData().toInt());
    m_dvLastVip = vip ? 1 : 0;
    m_dvVipLabel->setText(vip ? QStringLiteral("★ Client VIP") : QStringLiteral("Client standard"));
    m_dvRemise->setValue(vip ? kRemiseVip : 0.0); // déclenche updateDevis()
    updateDevis();
}

void OrderWindow::addDevisLine()
{
    const int id = m_dvMatiere->currentData().toInt();
    if (id <= 0) return;
    for (const AtelierMaterial &m : loadMatieres()) {
        if (m.id != id) continue;
        for (QuoteLine &l : m_dvLines) // même matière : on cumule la quantité
            if (l.matiereId == id) {
                l.qte += m_dvQte->value();
                updateDevis();
                return;
            }
        m_dvLines.push_back({m.id, m.nom, m_dvQte->value(), m.prix});
        break;
    }
    updateDevis();
}

void OrderWindow::removeDevisLine()
{
    const int r = m_dvTable->currentRow();
    if (r < 0 || r >= m_dvLines.size()) return;
    m_dvLines.removeAt(r);
    updateDevis();
}

void OrderWindow::updateDevis()
{
    if (!m_dvTable) return;
    double mat = 0;
    m_dvTable->setRowCount(0);
    for (const QuoteLine &l : m_dvLines) {
        const int r = m_dvTable->rowCount();
        m_dvTable->insertRow(r);
        setCell(m_dvTable, r, 0, l.nom);
        setCell(m_dvTable, r, 1, QString::number(l.qte, 'f', 1));
        setCell(m_dvTable, r, 2, fmtMoney(l.prix));
        setCell(m_dvTable, r, 3, fmtMoney(l.qte * l.prix));
        mat += l.qte * l.prix;
    }
    const double mo = m_dvHeures->value() * m_dvTaux->value();
    const double sub = mat + mo;
    const double rem = sub * m_dvRemise->value() / 100.0;

    m_dvMat->setText(fmtMoney(mat));
    m_dvMO->setText(fmtMoney(mo));
    m_dvSub->setText(fmtMoney(sub));
    m_dvRem->setText(QStringLiteral("- ") + fmtMoney(rem));
    m_dvTotal->setText(fmtMoney(sub - rem));
}

void OrderWindow::exportDevisPdf()
{
    if (m_dvLines.isEmpty() && m_dvHeures->value() <= 0) {
        QMessageBox::information(this, QStringLiteral("Devis"), QStringLiteral("Ajoutez des matières ou des heures de travail."));
        return;
    }
    double mat = 0;
    for (const QuoteLine &l : m_dvLines) mat += l.qte * l.prix;
    const double mo = m_dvHeures->value() * m_dvTaux->value();
    const double sub = mat + mo;
    const double rem = sub * m_dvRemise->value() / 100.0;
    const double total = sub - rem;

    const QDateTime now = QDateTime::currentDateTime();
    const QString num = QStringLiteral("DEV-") + now.toString(QStringLiteral("yyyyMMdd-HHmm"));
    const QString client = m_dvClient->currentText().toHtmlEscaped();
    const QString modele = m_dvModele->text().trimmed().isEmpty() ? QStringLiteral("—") : m_dvModele->text().trimmed().toHtmlEscaped();

    const QString cell = QStringLiteral("<td style=\"border-bottom:1px solid #ddd;\">");
    const QString cellR = QStringLiteral("<td align=\"right\" style=\"border-bottom:1px solid #ddd;\">");

    QString html = QString::fromUtf8(kPdfHead);
    html += QStringLiteral("<table width=\"100%\"><tr><td><h1 style=\"color:#8a6d2b; margin:0;\">DEVIS</h1></td>"
                           "<td align=\"right\">N° <b>%1</b><br>Date : %2<br>Validité : 30 jours</td></tr></table><hr>")
                .arg(num, now.toString(QStringLiteral("dd/MM/yyyy")));
    html += QStringLiteral("<p><b>Client :</b> %1<br><b>Modèle :</b> %2</p>").arg(client, modele);

    html += QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"6\">");
    html += QString::fromUtf8(kPdfHeaderRow);
    html += QStringLiteral("<th align=\"left\">Désignation</th><th align=\"right\">Quantité</th>"
                           "<th align=\"right\">Prix unitaire</th><th align=\"right\">Total</th></tr>");
    for (const QuoteLine &l : m_dvLines)
        html += QStringLiteral("<tr>") + cell + l.nom.toHtmlEscaped() + QStringLiteral("</td>")
                + cellR + QString::number(l.qte, 'f', 1) + QStringLiteral("</td>")
                + cellR + fmtMoney(l.prix) + QStringLiteral("</td>")
                + cellR + fmtMoney(l.qte * l.prix) + QStringLiteral("</td></tr>");
    html += QStringLiteral("<tr>") + cell + QStringLiteral("Main-d'œuvre</td>")
            + cellR + QString::number(m_dvHeures->value(), 'f', 1) + QStringLiteral(" h</td>")
            + cellR + fmtMoney(m_dvTaux->value()) + QStringLiteral(" / h</td>")
            + cellR + fmtMoney(mo) + QStringLiteral("</td></tr></table>");

    html += QStringLiteral("<br><table align=\"right\" cellspacing=\"0\" cellpadding=\"5\">");
    html += QStringLiteral("<tr><td>Matières</td><td align=\"right\">") + fmtMoney(mat) + QStringLiteral("</td></tr>");
    html += QStringLiteral("<tr><td>Main-d'œuvre</td><td align=\"right\">") + fmtMoney(mo) + QStringLiteral("</td></tr>");
    html += QStringLiteral("<tr><td>Sous-total</td><td align=\"right\">") + fmtMoney(sub) + QStringLiteral("</td></tr>");
    if (m_dvRemise->value() > 0)
        html += QStringLiteral("<tr><td>Remise VIP (%1 %)</td><td align=\"right\">- %2</td></tr>")
                    .arg(m_dvRemise->value(), 0, 'f', 1).arg(fmtMoney(rem));
    html += QStringLiteral("<tr bgcolor=\"#e5d5a1\"><td><b>TOTAL NET</b></td><td align=\"right\"><b style=\"font-size:13pt;\">")
            + fmtMoney(total) + QStringLiteral("</b></td></tr></table>");
    html += QStringLiteral("<p style=\"clear:both;\"></p><br><br><p style=\"color:#777; font-size:8.5pt;\">"
                           "Devis établi automatiquement à partir du prix des matières, des heures de travail et de la remise client.</p>");
    html += QStringLiteral("</body></html>");

    exportHtmlToPdf(this, html, num + QStringLiteral(".pdf"), false);
}

void OrderWindow::createCommandeFromDevis()
{
    if (m_dvLines.isEmpty() && m_dvHeures->value() <= 0) {
        QMessageBox::information(this, QStringLiteral("Devis"), QStringLiteral("Ajoutez des matières ou des heures de travail."));
        return;
    }
    double mat = 0;
    for (const QuoteLine &l : m_dvLines) mat += l.qte * l.prix;
    const double sub = mat + m_dvHeures->value() * m_dvTaux->value();
    const double total = sub - sub * m_dvRemise->value() / 100.0;

    AtelierOrder c;
    c.clientId = m_dvClient->currentData().toInt();
    c.modele = m_dvModele->text().trimmed();
    c.montant = std::round(total * 100) / 100.0;
    c.acompte = std::round(total * 30) / 100.0; // acompte suggéré : 30 %
    c.livraison = QDate::currentDate().addDays(21);
    if (!commandeDialog(this, QStringLiteral("Nouvelle commande depuis le devis"), c, m_clients)) return;

    if (!insertCommande(c)) return;
    refreshAll();
    setPage(1);
}
