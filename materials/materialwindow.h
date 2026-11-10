#pragma once
#include "inventoryrepository.h"
#include <QMainWindow>
#include <QImage>
#include <QList>

namespace Ui { class MaterialWindow; }
class QPushButton;
class QTableWidget;

// Module « Gestion des matières » : stock, réapprovisionnement, lots & QR, statistiques.
class MaterialWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MaterialWindow(InventoryRepository &repository, QWidget *parent = nullptr);
    ~MaterialWindow() override;
    void setPage(int index);
    // Intégration : la fenêtre principale remplace la barre latérale par une rangée de boutons.
    void setSidebarVisible(bool visible);
    QList<QPushButton *> navButtons() const;
    QPushButton *helpButton() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    Ui::MaterialWindow *ui;
    InventoryRepository &repo;
    QVector<Material> materials;   // toutes les matières, relues par refresh()
    qint64 editingId = 0;          // matière ouverte dans le formulaire (0 = nouvelle)
    QImage currentQr;              // QR du lot affiché

    // Affichage
    void refresh();
    void updateMetrics();
    void updateInventory();
    void updateAlerts();
    void updateLotList();
    void updateLot();
    void updateStats();
    void materialSelected();
    void alertSelected();
    void showHelp();

    // Actions
    void edit(qint64 id = 0);
    void editSelected();
    void save();
    void removeSelected();
    void moveStock(qint64 id, bool receiptOnly = false);
    void purchaseOrder();
    void selectQrLot(qint64 id);
    void scanCode(const QString &code);
    void scanImage();
    void consume();
    void saveQr();
    void printLabel();
    void exportInventory();
    void exportStats();

    // Outils
    qint64 selectedId(QTableWidget *table) const;
    Material currentLot();
    QVector<Material> filteredMaterials() const;
    QString pdfPath(const QString &suggestion);
    void notify(const QString &message);
    void failure(const QString &message);
};
