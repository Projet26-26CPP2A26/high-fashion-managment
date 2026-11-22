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
class AtelierClient {
public:
    AtelierClient() = default;
    AtelierClient(int id, const QString &nom, bool vip = false)
        : m_id(id), m_nom(nom), m_vip(vip) {}

    int id() const { return m_id; }
    QString nom() const { return m_nom; }
    bool vip() const { return m_vip; }

    void setId(int id) { m_id = id; }
    void setNom(const QString &nom) { m_nom = nom; }
    void setVip(bool vip) { m_vip = vip; }

private:
    int m_id = 0;
    QString m_nom;
    bool m_vip = false;
};

// Classe métier « Commande » : attributs + CRUD
class AtelierOrder {
public:
    AtelierOrder() = default;
    AtelierOrder(int id, const QDate &date, int clientId, const QString &modele,
                 double montant, double acompte, const QDate &livraison, const QString &statut)
        : m_id(id), m_date(date), m_clientId(clientId), m_modele(modele),
        m_montant(montant), m_acompte(acompte), m_livraison(livraison), m_statut(statut) {}

    // Getters
    int id() const { return m_id; }
    QDate date() const { return m_date; }
    int clientId() const { return m_clientId; }
    QString modele() const { return m_modele; }
    double montant() const { return m_montant; }
    double acompte() const { return m_acompte; }
    QDate livraison() const { return m_livraison; }
    QString statut() const { return m_statut; }

    // Setters
    void setId(int id) { m_id = id; }
    void setDate(const QDate &d) { m_date = d; }
    void setClientId(int id) { m_clientId = id; }
    void setModele(const QString &m) { m_modele = m; }
    void setMontant(double m) { m_montant = m; }
    void setAcompte(double a) { m_acompte = a; }
    void setLivraison(const QDate &d) { m_livraison = d; }
    void setStatut(const QString &s) { m_statut = s; }

    // CRUD
    bool ajouter() const;
    bool modifier() const;
    static bool supprimer(int id);
    static QVector<AtelierOrder> afficher();
    static QString derniereErreur();

private:
    int m_id = 0;
    QDate m_date = QDate::currentDate();
    int m_clientId = 0;
    QString m_modele;
    double m_montant = 0;
    double m_acompte = 0;
    QDate m_livraison = QDate::currentDate().addDays(14);
    QString m_statut = QStringLiteral("En attente");

    static QString s_erreur;
};

class AtelierMaterial {
public:
    AtelierMaterial() = default;

    int id() const { return m_id; }
    QString nom() const { return m_nom; }
    QString type() const { return m_type; }
    QString couleur() const { return m_couleur; }
    double quantite() const { return m_quantite; }
    double seuil() const { return m_seuil; }
    double prix() const { return m_prix; }
    QString fournisseur() const { return m_fournisseur; }
    QString lot() const { return m_lot; }

    void setId(int id) { m_id = id; }
    void setNom(const QString &n) { m_nom = n; }
    void setType(const QString &t) { m_type = t; }
    void setCouleur(const QString &c) { m_couleur = c; }
    void setQuantite(double q) { m_quantite = q; }
    void setSeuil(double s) { m_seuil = s; }
    void setPrix(double p) { m_prix = p; }
    void setFournisseur(const QString &f) { m_fournisseur = f; }
    void setLot(const QString &l) { m_lot = l; }

private:
    int m_id = 0;
    QString m_nom;
    QString m_type;
    QString m_couleur;
    double m_quantite = 0;
    double m_seuil = 0;
    double m_prix = 0;
    QString m_fournisseur;
    QString m_lot;
};

class QuoteLine {
public:
    QuoteLine() = default;
    QuoteLine(int matiereId, const QString &nom, double qte, double prix)
        : m_matiereId(matiereId), m_nom(nom), m_qte(qte), m_prix(prix) {}

    int matiereId() const { return m_matiereId; }
    QString nom() const { return m_nom; }
    double qte() const { return m_qte; }
    double prix() const { return m_prix; }

    void setMatiereId(int id) { m_matiereId = id; }
    void setNom(const QString &n) { m_nom = n; }
    void setQte(double q) { m_qte = q; }
    void setPrix(double p) { m_prix = p; }

private:
    int m_matiereId = 0;
    QString m_nom;
    double m_qte = 0;
    double m_prix = 0;
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
class OrderWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit OrderWindow(QWidget *parent = nullptr);

    bool ready() const { return m_ready; }
    void setPage(int index);
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
    bool openDatabase();
    QSqlDatabase ordersDb() const;
    void createTables();
    void seedIfEmpty();
    bool insertCommande(const AtelierOrder &c);
    QVector<AtelierClient> loadClients() const;
    QVector<AtelierOrder> loadCommandes() const;
    QVector<AtelierMaterial> loadMatieres() const;
    bool isVip(int clientId) const;
    QString clientLabel(int clientId) const;

    void buildUi();
    QWidget *buildSidebar();
    QWidget *buildTopbar();
    QWidget *buildDashboardPage();
    QWidget *buildCommandesPage();
    QWidget *buildDevisPage();

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
    int m_dvLastVip = -1;
};
