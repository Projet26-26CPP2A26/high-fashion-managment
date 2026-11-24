#pragma once
#include <QList>
#include <QMainWindow>
#include <QPushButton>

namespace Ui {
class EmployeeWindow;
}
class EmployeeModule;
class ActivityBoard;
class CompanyLink;
class BadgePage;
class ReviewBoard;
class ActivityStore;
class QStackedWidget;

class EmployeeWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit EmployeeWindow(ActivityStore &store, bool demo = false, QWidget *parent = nullptr);
    ~EmployeeWindow() override;
    void setPage(int index);
    void showReviews();
    void showBadges();
    bool ready() const;

    void setSidebarVisible(bool visible);
    QList<QPushButton *> navButtons() const;
    QPushButton *helpButton() const;
    EmployeeModule *module() const
    {
        return employeeModule;
    }
signals:
    void closed(bool logout);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::EmployeeWindow *ui;
    EmployeeModule *employeeModule;
    ActivityBoard *board;
    ReviewBoard *reviews;
    QPushButton *reviewButton, *badgeButton;
    BadgePage *badges;
    CompanyLink *link = nullptr;
    QStackedWidget *pages;
    bool logoutRequested = false, leaving = false;
    int lastEntry = 0, refusals = 0;
};
