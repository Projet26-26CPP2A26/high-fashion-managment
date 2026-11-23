#pragma once
#include "activitystore.h"
#include <QWidget>

class ActivityBoard;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class StarRating;

class ReviewBoard : public QWidget {
    Q_OBJECT
public:
    explicit ReviewBoard(ActivityBoard &board, QWidget *parent = nullptr);
    void enter();

private:
    void reload();
    void show(int taskId);
    void decide(bool accept);

    ActivityBoard &m_board;
    QVector<WorkTask> m_tasks;
    WorkTask m_current;
    QListWidget *m_list;
    QStackedWidget *m_pages;
    QLabel *m_title, *m_info, *m_steps, *m_note;
    QLabel *m_kpi[3];
    QPushButton *m_open;
    StarRating *m_stars;
    QPlainTextEdit *m_feedback;
};
