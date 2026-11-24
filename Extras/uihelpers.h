#pragma once
#include "dialogskin.h"
#include "employee.h"
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QLinearGradient>
#include <QGridLayout>
#include <QLocale>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QSet>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace UiHelpers {
inline QString duration(int minutes) {return QStringLiteral("%1 h %2").arg(minutes/60).arg(minutes%60,2,10,QChar('0'));}
inline QString hm(const QTime &time) {return time.isValid()?time.toString("HH:mm"):QStringLiteral("—");}
inline QString availability(const Employee &e) {return e.available()?QStringLiteral("Disponible"):QStringLiteral("Indisponible");}
inline QString archived(const QString &name,int id) {return id?name:name+QStringLiteral(" (supprimé)");}
inline void setupTable(QTableWidget *t,const QList<int> &widths) {
    t->verticalHeader()->hide(); t->verticalHeader()->setDefaultSectionSize(42); t->setFrameShape(QFrame::NoFrame); t->setFocusPolicy(Qt::NoFocus); t->setWordWrap(false);
    t->horizontalHeader()->setStretchLastSection(true); t->setSortingEnabled(false);
    t->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    for(int i=0;i<widths.size();++i) t->setColumnWidth(i,widths[i]);
}

inline QColor statusColor(const QString &text) {
    if(text==QLatin1String("Disponible")||text==QLatin1String("Terminée")||text==QLatin1String("Terminé")||text==QLatin1String("Suffisante")) return QColor("#7fc08a");
    if(text==QLatin1String("Indisponible")||text==QLatin1String("Insuffisante")||text==QLatin1String("À corriger")) return QColor("#e08f8f");
    if(text==QLatin1String("En cours")) return QColor("#e6d3a3");
    return QColor();
}
inline void addRow(QTableWidget *t,const QStringList &values,int id=0) {
    const int row=t->rowCount(); t->insertRow(row);
    for(int col=0;col<values.size();++col) {
        auto *item=new QTableWidgetItem(values[col]); item->setData(Qt::UserRole,id);
        const QColor colour=statusColor(values[col]); if(colour.isValid()) item->setForeground(colour);
        t->setItem(row,col,item);
    }
}
inline QString initials(const QString &name) {
    QString out; for(const QString &part:name.split(QLatin1Char(' '),Qt::SkipEmptyParts)) {if(out.size()<2) out+=part.at(0).toUpper();}
    return out.isEmpty()?QStringLiteral("?"):out;
}
class BarRow {
public:
    BarRow(const QString &label,double value,const QString &text,bool high):m_label(label),m_value(value),m_text(text),m_high(high) {}
    QString label() const {return m_label;}
    double value() const {return m_value;}
    QString text() const {return m_text;}
    bool high() const {return m_high;}
private:
    QString m_label;
    double m_value;
    QString m_text;
    bool m_high;
};

inline QPixmap barsImage(const QVector<BarRow> &rows,double maxValue,int width,int labelWidth,qreal ratio) {
    const int rowHeight=30,height=std::max(70,int(rows.size())*rowHeight+6);
    QPixmap pixmap(int(width*ratio),int(height*ratio)); pixmap.setDevicePixelRatio(ratio); pixmap.fill(Qt::transparent);
    QPainter p(&pixmap); p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::TextAntialiasing); p.setFont(QFont(QStringLiteral("Segoe UI"),9));
    const int barX=labelWidth+10,textWidth=46,barWidth=std::max(40,width-barX-textWidth-8); const QFontMetrics metrics(p.font()); int y=0;
    for(const auto &r:rows) {
        p.setPen(QColor("#d9d5cb")); p.drawText(QRect(0,y,labelWidth,rowHeight),Qt::AlignLeft|Qt::AlignVCenter,metrics.elidedText(r.label(),Qt::ElideRight,labelWidth));
        const QRectF track(barX,y+rowHeight/2.0-4,barWidth,8); p.setPen(Qt::NoPen); p.setBrush(QColor("#34343a")); p.drawRoundedRect(track,4,4);
        const double length=barWidth*std::clamp(maxValue>0?r.value()/maxValue:0.0,0.0,1.0);
        if(length>0) {
            QLinearGradient gradient(barX,0,barX+barWidth,0);
            if(r.high()) {gradient.setColorAt(0,QColor("#c27575")); gradient.setColorAt(1,QColor("#e8a3a3"));} else {gradient.setColorAt(0,QColor("#bfa370")); gradient.setColorAt(1,QColor("#ecdcae"));}
            p.setBrush(gradient); p.drawRoundedRect(QRectF(barX,track.top(),std::max(8.0,length),8),4,4);
        }
        p.setPen(QColor(r.high()?"#e8a3a3":"#e2cf9f")); p.drawText(QRect(barX+barWidth+8,y,textWidth,rowHeight),Qt::AlignLeft|Qt::AlignVCenter,r.text());
        y+=rowHeight;
    }
    if(rows.isEmpty()) {p.setPen(QColor("#b3aea3")); p.drawText(QRect(0,0,width,height),Qt::AlignCenter,QStringLiteral("Aucun employé enregistré"));}
    return pixmap;
}

inline QPixmap hoursChartImage(const QVector<double> &values,const QStringList &labels,int width,int height,qreal ratio) {
    QPixmap pixmap(int(width*ratio),int(height*ratio)); pixmap.setDevicePixelRatio(ratio); pixmap.fill(Qt::transparent);
    QPainter p(&pixmap); p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::TextAntialiasing); p.setFont(QFont(QStringLiteral("Segoe UI"),8));
    double top=1; for(double v:values) top=std::max(top,v); top=std::ceil(top/2.0)*2.0; if(top<4) top=4;
    const QRectF plot(46,10,width-62,height-44); const int n=values.size();
    for(int i=0;i<=4;++i) {
        const double y=plot.bottom()-plot.height()*i/4.0;
        p.setPen(QPen(QColor("#323236"),1)); p.drawLine(QPointF(plot.left(),y),QPointF(plot.right(),y));
        p.setPen(QColor("#b3aea3")); p.drawText(QRectF(0,y-9,plot.left()-8,18),Qt::AlignRight|Qt::AlignVCenter,QStringLiteral("%1 h").arg(top*i/4.0,0,'f',top*i/4.0==int(top*i/4.0)?0:1));
    }
    QVector<QPointF> pts; for(int i=0;i<n;++i) pts.append(QPointF(plot.left()+plot.width()*i/std::max(1,n-1),plot.bottom()-plot.height()*values[i]/top));
    for(int i=0;i<n;++i) {p.setPen(QColor("#b3aea3")); p.drawText(QRectF(pts[i].x()-30,plot.bottom()+8,60,18),Qt::AlignCenter,labels[i]);}
    if(n<2) return pixmap;
    QPainterPath line; line.moveTo(pts[0]);
    for(int i=0;i<n-1;++i) {
        const QPointF p0=pts[std::max(0,i-1)],p1=pts[i],p2=pts[i+1],p3=pts[std::min(n-1,i+2)];
        QPointF c1=p1+(p2-p0)/6.0,c2=p2-(p3-p1)/6.0;
        c1.setY(std::clamp(c1.y(),plot.top(),plot.bottom())); c2.setY(std::clamp(c2.y(),plot.top(),plot.bottom()));
        line.cubicTo(c1,c2,p2);
    }
    QPainterPath area=line; area.lineTo(pts.last().x(),plot.bottom()); area.lineTo(pts.first().x(),plot.bottom()); area.closeSubpath();
    QLinearGradient fill(0,plot.top(),0,plot.bottom()); fill.setColorAt(0,QColor(230,211,163,95)); fill.setColorAt(1,QColor(230,211,163,0));
    p.setPen(Qt::NoPen); p.setBrush(fill); p.drawPath(area);
    QPen pen(QColor("#ecdcae"),2.2); pen.setCapStyle(Qt::RoundCap); p.setPen(pen); p.setBrush(Qt::NoBrush); p.drawPath(line);
    p.setPen(QPen(QColor("#232326"),2)); p.setBrush(QColor("#ecdcae")); p.drawEllipse(pts.last(),4.5,4.5);
    return pixmap;
}
inline bool confirm(QWidget *parent,const QString &text) {
    return DialogSkin::ask(parent,"Confirmation",text,"Confirmer","Annuler");
}
inline QString document(const QString &title,const QString &body) {
    return QStringLiteral("<html><body style='font-family:Segoe UI;font-size:9pt;color:#222'><h1>AUREA — %1</h1><p>Atelier de couture · %2</p>%3</body></html>")
        .arg(title.toHtmlEscaped(),QDate::currentDate().toString("dd/MM/yyyy"),body);
}
inline QString headers(const QStringList &values) {QString out="<tr bgcolor='#e5d5a1'>"; for(const auto &v:values) out+="<th align='left'>"+v.toHtmlEscaped()+"</th>"; return out+"</tr>";}
inline QString htmlRow(const QStringList &values) {QString out="<tr>"; for(const auto &v:values) out+="<td>"+v.toHtmlEscaped()+"</td>"; return out+"</tr>";}
}
