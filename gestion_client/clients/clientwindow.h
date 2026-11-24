#ifndef CLIENTWINDOW_H
#define CLIENTWINDOW_H

#include <QMainWindow>
#include <QList>

class QPushButton;
class QTableWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class ClientWindow; }
QT_END_NAMESPACE

// Module « Gestion des clients » (VUE) : aucune requête SQL ici,
// tout passe par la classe Client (MODÈLE).
class ClientWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ClientWindow(QWidget *parent = nullptr);
    ~ClientWindow();

    // Intégration : la fenêtre principale remplace la barre latérale par une rangée de boutons horizontale.
    void setSidebarVisible(bool visible);
    QList<QPushButton *> navButtons() const { return m_nav; }

private slots:
    // Auto-connected to the buttons of the .ui (on_<objectName>_clicked)
    void on_btnNouveau_clicked();
    void on_btnModifier_clicked();
    void on_btnSupprimer_clicked();
    void on_btnVoirMesures_clicked();
    void on_btnExportPdf_clicked();
    void on_btnEnregistrer_clicked();
    void on_btnAnnuler_clicked();
    void on_btnAjouterMesure_clicked();
    void on_btnTaille_clicked();         // métier avancé : taille recommandée
    void on_btnRappel_clicked();
    void on_btnAjouterCommande_clicked();
    void on_btnRelance_clicked();        // métier avancé : clients inactifs

    // Data refresh
    void refreshClients();       // CRUD "afficher" + rechercher + trier + KPI
    void refreshMesures();       // historique des mesures
    void refreshFidelite();      // classement fidélité
    void buildCharts();          // statistiques

private:
    // navigation
    void showPage(int index);
    void startAdd();
    bool startEdit(int id);      // false si le client n'a pas pu être chargé

    // setup
    void setupLogo();
    void setupTables();
    void setupCombos();
    void reloadClientCombos();

    // helpers
    int  currentId(QTableWidget *t) const;
    void openMail(const QString &to, const QString &subject, const QString &body);

    Ui::ClientWindow *ui;
    int m_editId = -1;                  // -1 = ajout, sinon id du client en modification
    QList<QPushButton *> m_nav;
};

#endif // CLIENTWINDOW_H
