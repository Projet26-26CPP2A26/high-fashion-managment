#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>

class QPushButton;
class QTableWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

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
    void on_btnRappel_clicked();
    void on_btnAjouterCommande_clicked();
    void on_btnAnniv_clicked();

    // Data refresh
    void refreshClients();       // CRUD "afficher" + rechercher + trier + KPI
    void refreshMesures();       // historique des mesures
    void refreshFidelite();      // classement fidélité + anniversaires
    void buildCharts();          // statistiques

private:
    // navigation
    void showPage(int index);
    void startAdd();
    void startEdit(int id);

    // setup
    void setupLogo();
    void setupTables();
    void setupCombos();
    void reloadClientCombos();

    // helpers
    int  currentId(QTableWidget *t) const;
    bool recomputeScore(int clientId);          // true si le client vient de devenir VIP
    QStringList birthdaysToday() const;
    void openMail(const QString &to, const QString &subject, const QString &body);

    Ui::MainWindow *ui;
    int m_editId = -1;                  // -1 = ajout, sinon id du client en modification
    QList<QPushButton *> m_nav;
};

#endif // MAINWINDOW_H
