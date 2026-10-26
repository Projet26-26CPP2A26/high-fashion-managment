#pragma once
#include <QColor>
#include <QDate>
#include <QList>
#include <QMainWindow>
#include <QSize>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QTableWidgetItem>
#include <QVector>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QStackedWidget;
class QTableWidget;
class QTimer;

// ------------------------------------------------------------ Données
struct AtelierClient {
    int id = 0;
    QString nom;
    bool vip = false;
};

struct AtelierOrder {
    int id = 0;
    QDate date = QDate::currentDate();
    int clientId = 0;
    QString modele;
    double montant = 0;
    double acompte = 0;
    QDate livraison = QDate::currentDate().addDays(14);
    QString statut = QStringLiteral("En attente");
};

struct AtelierMaterial {
    int id = 0;
    QString nom;
    QString type;
    QString couleur;
    double quantite = 0;
    double seuil = 0;
    double prix = 0;
    QString fournisseur;
    QString lot;
};

struct QuoteLine {
    int matiereId = 0;
    QString nom;
    double qte = 0;
    double prix = 0;
};

// Élément de tableau triable sur une clé numérique (Qt::UserRole + 1)
class NumericItem : public QTableWidgetItem {
public:
    using QTableWidgetItem::QTableWidgetItem;
    bool operator<(const QTableWidgetItem &other) const override
    {
        return data(Qt::UserRole + 1).toDouble() < other.data(Qt::UserRole + 1).toDouble();
    }
};

// Courbe lissée avec dégradé (chiffre d'affaires)
class RevenueChart : public QWidget {
public:
    explicit RevenueChart(QWidget *parent = nullptr);
    void setData(const QStringList &labels, const QVector<double> &values, const QString &unit);
    QSize sizeHint() const override { return QSize(400, 240); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QStringList m_labels;
    QVector<double> m_values;
    QString m_unit;
};

// Histogramme (commandes par statut)
class StatusChart : public QWidget {
public:
    explicit StatusChart(QWidget *parent = nullptr);
    void setData(const QStringList &labels, const QVector<double> &values, const QVector<QColor> &colors);
    QSize sizeHint() const override { return QSize(400, 220); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QStringList m_labels;
    QVector<double> m_values;
    QVector<QColor> m_colors;
};

// Module « Gestion des commandes » (tableau de bord, commandes, devis automatique).
// Les clients sont ceux du module Clients (même base SQLite) ; la remise VIP suit leur statut VIP.
class OrderWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit OrderWindow(QWidget *parent = nullptr);

    bool ready() const { return m_ready; }
    void setPage(int index);
    // Intégration : la fenêtre principale remplace la barre latérale par une rangée de boutons horizontale.
    void setSidebarVisible(bool visible);
    QList<QPushButton *> navButtons() const { return m_navButtons; }
    QPushButton *helpButton() const { return m_help; }

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void updateClock();
    void showHelp();

    // commandes
    void addCommande();
    void editCommande();
    void deleteCommande();
    void applyCommandeFilter();
    void applyCommandeSort();
    void exportCommandesPdf();

    // devis
    void onDevisClientChanged();
    void addDevisLine();
    void removeDevisLine();
    void updateDevis();
    void exportDevisPdf();
    void createCommandeFromDevis();

private:
    // base de données (connexion par défaut = base du module Clients)
    bool openDatabase();
    QSqlDatabase ordersDb() const;   // connexion Oracle (commandes)
    void createTables();
    void seedIfEmpty();
    bool insertCommande(const AtelierOrder &c);
    QVector<AtelierClient> loadClients() const;
    QVector<AtelierOrder> loadCommandes() const;
    QVector<AtelierMaterial> loadMatieres() const;
    bool isVip(int clientId) const;
    QString clientLabel(int clientId) const;

    // interface
    void buildUi();
    QWidget *buildSidebar();
    QWidget *buildTopbar();
    QWidget *buildDashboardPage();
    QWidget *buildCommandesPage();
    QWidget *buildDevisPage();

    // rafraîchissement
    void refreshAll();
    void refreshDashboard();
    void refreshCommandes();
    void reloadCombos();

    QSqlDatabase m_db;
    QString m_dbPath;
    bool m_ready = false;
    QVector<AtelierClient> m_clients;

    QFrame *m_sidebar = nullptr;
    QStackedWidget *m_pages = nullptr;
    QList<QPushButton *> m_navButtons;
    QPushButton *m_help = nullptr;
    QTimer *m_clock = nullptr;
    QLabel *m_pageTitle = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_footer = nullptr;
    QLineEdit *m_globalSearch = nullptr;
    QLabel *m_dateLabel = nullptr;

    // Tableau de bord
    QLabel *m_statTotal = nullptr, *m_statTotalSub = nullptr;
    QLabel *m_statEnCours = nullptr, *m_statEnCoursSub = nullptr;
    QLabel *m_statLivrees = nullptr, *m_statLivreesSub = nullptr;
    QLabel *m_statCA = nullptr, *m_statCASub = nullptr;
    QPushButton *m_alertBtn = nullptr;
    RevenueChart *m_caChart = nullptr;
    StatusChart *m_statutChart = nullptr;
    QTableWidget *m_dashRetards = nullptr;
    QTableWidget *m_dashRecent = nullptr;

    // Commandes
    QLineEdit *m_cmdFilter = nullptr;
    QComboBox *m_cmdSort = nullptr;
    QPushButton *m_cmdSortDir = nullptr;
    QTableWidget *m_cmdTable = nullptr;

    // Devis
    QComboBox *m_dvClient = nullptr;
    QLineEdit *m_dvModele = nullptr;
    QDoubleSpinBox *m_dvHeures = nullptr;
    QDoubleSpinBox *m_dvTaux = nullptr;
    QDoubleSpinBox *m_dvRemise = nullptr;
    QComboBox *m_dvMatiere = nullptr;
    QDoubleSpinBox *m_dvQte = nullptr;
    QTableWidget *m_dvTable = nullptr;
    QLabel *m_dvMat = nullptr, *m_dvMO = nullptr, *m_dvSub = nullptr, *m_dvRem = nullptr, *m_dvTotal = nullptr;
    QLabel *m_dvVipLabel = nullptr;
    QVector<QuoteLine> m_dvLines;
    int m_dvLastVip = -1;   // -1 : inconnu ; sert à réappliquer la remise si le statut VIP change dans le module Clients
};
