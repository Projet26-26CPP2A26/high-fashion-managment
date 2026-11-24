#pragma once
#include "AnimationSpeed.h"
#include "activitystore.h"
#include <QWidget>
#include <functional>

class QLabel;
class QPushButton;
class QScrollArea;
class QTimer;

class PassageList : public QWidget {
public:
    explicit PassageList(QWidget *parent = nullptr);
    void setEntries(const QVector<AccessEntry> &entries, bool animate);
    std::function<WorkSession(const AccessEntry &)> details;
    void toggle(int index);
    void refresh();
    QString emptyText = QStringLiteral("Les passages apparaîtront ici en direct.");

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    bool clickable(const AccessEntry &e) const
    {
        return !e.who.isEmpty() && e.who != "ADMIN" && e.kind != "REFUSE";
    }
    int indexAt(int y) const;
    int openIndex() const;
    void fit();
    void paintDetails(QPainter &p, const QRectF &box, double alpha) const;
    void paintRow(QPainter &p, const AccessEntry &e, const QRectF &row, double alpha, double shine) const;
    static constexpr int Row = 64, Extra = 104;
    QVector<AccessEntry> m_entries;
    QTimer *m_timer;
    Motion::Tween m_shift, m_shine, m_expand;
    WorkSession m_day;
    int m_fresh = 0, m_open = 0;
};

class PassageFeed : public QWidget {
public:
    explicit PassageFeed(ActivityStore &store, EntryFilter filter = EntryFilter::Passages, QWidget *parent = nullptr);
    void reload();

private:
    void countPresent();
    ActivityStore &m_store;
    PassageList *m_list;
    QLabel *m_title;
    QPushButton *m_filters[3];
    EntryFilter m_filter;
    QString m_via;
    int m_last = -1;
};
