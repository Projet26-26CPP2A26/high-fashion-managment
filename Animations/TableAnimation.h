#pragma once
#include <QElapsedTimer>
#include <QHash>
#include <QPixmap>
#include <QStyledItemDelegate>
#include <QTimer>

class QTableWidget;

// Tableaux : lignes qui arrivent en cascade, survol doux, icône de la matière, jauge du stock, pastille d'état.
class TableAnimation : public QStyledItemDelegate {
public:
    explicit TableAnimation(QTableWidget *table);
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    QString header(int column) const;
    void wake();
    double glow(int row) const;
    QPixmap icon(const QString &type) const;

    QTableWidget *m_table;
    QElapsedTimer m_enter, m_life;
    QTimer m_timer, m_pulse;
    int m_hover = -1;
    mutable QHash<QString, QPixmap> m_icons;
    QHash<int, double> m_glow;
};
