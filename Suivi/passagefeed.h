#pragma once
#include "AnimationSpeed.h"
#include "activitystore.h"
#include <QWidget>
#include <functional>

class QLabel;
class QPushButton;
class QScrollArea;

class PassageList : public QWidget {
public:
    explicit PassageList(QWidget *parent = nullptr);
    void setEntries(const QVector<AccessEntry> &entries, bool animate);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void paintRow(QPainter &p, const AccessEntry &e, const QRectF &row, double alpha, double shine) const;
    static constexpr int Row = 64;
    QVector<AccessEntry> m_entries;
    Motion::Tween m_shift, m_shine;
    int m_fresh = 0;
};

class PassageFeed : public QWidget {
public:
    explicit PassageFeed(ActivityStore &store, QWidget *parent = nullptr);
    void reload();

private:
    void countPresent();
    ActivityStore &m_store;
    PassageList *m_list;
    QLabel *m_title;
    QPushButton *m_filters[3];
    QString m_via;
    int m_last = -1;
};
