#pragma once
#include "inventoryrepository.h"
#include <QMainWindow>
#include <QImage>
#include <QList>

namespace Ui { class MaterialWindow; }
class QPushButton;
class QTableWidget;

// Module « Gestion des matières » (stock, réapprovisionnement, lots & QR, statistiques, mouvements).
// Même thème sombre / or que les modules Employés, Clients et Commandes (theme.qss).
class MaterialWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MaterialWindow(InventoryRepository &repository,QWidget *parent=nullptr);
    ~MaterialWindow() override;
    void setPage(int index);
    void renderPreviews(const QString &directory);
    // Intégration : la fenêtre principale remplace la barre latérale par une rangée de boutons horizontale.
    void setSidebarVisible(bool visible);
    QList<QPushButton *> navButtons() const;
    QPushButton *helpButton() const;
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    Ui::MaterialWindow *ui;
    InventoryRepository &repo;
    QVector<Material> materials;
    qint64 editingId=0;
    QImage currentQr;
    QString currentPhoneUrl;
    void refresh();
    void updateInventory();
    void updateStats();
    void updateLot();
    void selectQrLot(qint64 id);
    qint64 selectedId(QTableWidget *table) const;
    QVector<Material> filteredMaterials() const;
    void edit(qint64 id=0);
    void save();
    void removeSelected();
    void moveStock(qint64 id,bool receiptOnly=false);
    void purchaseOrder();
    void scanCode(const QString &code);
    void scanImage();
    void consume();
    void exportInventory();
    void exportStats();
    void saveQr();
    void printLabel();
    QString pdfPath(const QString &suggestion);
    void notify(const QString &message);
    void failure(const QString &message);
};
