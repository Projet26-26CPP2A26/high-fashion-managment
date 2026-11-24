#pragma once
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QVariantAnimation>
#include <QVector>

#include <QLabel>
class QTableWidget;
class QWidget;

class TablePolish : public QObject {
    Q_OBJECT
public:
    static void apply(QWidget *root);
    qreal appear(int row) const;
    static void flash(QTableWidget *table, int row);
    qreal glow(int row) const;
    int hovered() const { return m_hovered; }

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    explicit TablePolish(QTableWidget *table);
    bool alive() const;
    void fit();
    void updateEmpty();

    QTableWidget *m_table;
    QPointer<QLabel> m_empty;
    QWidget *m_emptyIcon = nullptr;
    QVector<int> m_base;
    QVariantAnimation m_animation, m_glow;
    int m_glowRow = -1;
    int m_hovered = -1;
};
