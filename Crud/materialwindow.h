#pragma once
#include "arduino.h"
#include "inventoryrepository.h"
#include <QMainWindow>
#include <QImage>
#include <QList>
#include <QSet>

namespace Ui { class MaterialWindow; }
class BornePage;
class LinkLight;
class QLabel;
class QPushButton;
class QTableWidget;
class QrAnimation;

class MaterialWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MaterialWindow(QWidget *parent=nullptr);
    ~MaterialWindow() override;
    void setPage(int index);
    void setSidebarVisible(bool visible);
    QList<QPushButton *> navButtons() const;
    QPushButton *helpButton() const;
    void playOpening();
protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
private:
    Ui::MaterialWindow *ui;
    InventoryRepository repo;
    QVector<Material> materials;
    QSet<int> lowIds;
    int editingId=0;
    QImage currentQr;
    QString currentQrText;
    void refresh();
    void updateInventory();
    void updateStats();
    void updateLot();
    void selectQrLot(int id);
    int selectedId(QTableWidget *table) const;
    QVector<Material> filteredMaterials() const;
    void edit(int id=0);
    void save();
    void removeSelected();
    void changeStock(int id,bool receiptOnly=false);
    void purchaseOrder();
    Arduino A;
    QByteArray arduinoData;
    QLabel *arduinoLabel;
    LinkLight *arduinoLight;
    BornePage *borne;
    QPushButton *navBorne;
    QrAnimation *qrAnimation;
    bool opening=false, leaving=false;
    QVector<Material> waitingAlerts;
    void setArduinoState(bool on,const QString &text);
    void connectArduino();
    void readArduino();
    QByteArray arduinoAnswer(const QString &message);
    void orderAll();
    void showAlert(const QVector<Material> &list);
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
