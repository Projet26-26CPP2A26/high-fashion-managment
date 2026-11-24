#pragma once
#include "inventoryrepository.h"
#include <QAbstractItemView>
#include <QHeaderView>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>

inline QTableWidgetItem *cell(const QString &text,int id=0) {
    auto *item=new QTableWidgetItem(text); item->setData(Qt::UserRole,id); return item;
}
inline QTableWidgetItem *number(double value,int id=0) {
    auto *item=new QTableWidgetItem; item->setData(Qt::DisplayRole,value); item->setData(Qt::UserRole,id);
    item->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter); return item;
}
inline void configure(QTableWidget *table,const QList<int> &widths) {
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    table->verticalHeader()->setDefaultSectionSize(52); table->verticalHeader()->hide();
    table->setWordWrap(false); table->setAlternatingRowColors(true); table->setSortingEnabled(false);
    for(int i=0;i<widths.size();++i) table->setColumnWidth(i,widths[i]);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
}
inline bool confirm(QWidget *parent,const QString &title,const QString &text) {
    QMessageBox box(QMessageBox::Question,title,text,QMessageBox::NoButton,parent);
    auto *yes=box.addButton(QStringLiteral("Confirmer"),QMessageBox::AcceptRole);
    auto *no=box.addButton(QStringLiteral("Annuler"),QMessageBox::RejectRole);
    box.setDefaultButton(no); box.exec(); return box.clickedButton()==yes;
}
inline QString state(const Material &m) {
    return m.quantity()==0?QStringLiteral("Rupture"):m.low()?QStringLiteral("À commander"):QStringLiteral("Disponible");
}
