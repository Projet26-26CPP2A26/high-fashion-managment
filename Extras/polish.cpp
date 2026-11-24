#include "polish.h"
#include "employeetile.h"
#include "LogoAnimation.h"
#include "AnimationSpeed.h"
#include <QEvent>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <algorithm>

namespace {
QColor pillColor(const QString &t)
{
    if (t == QStringLiteral("Arrivée") || t == QStringLiteral("Disponible") || t == QStringLiteral("Suffisante") || t == QStringLiteral("Terminée") || t == QStringLiteral("Terminé")) return QColor("#7fc08a");
    if (t == QStringLiteral("Refusé") || t == QStringLiteral("Indisponible") || t == QStringLiteral("Insuffisante") || t == QStringLiteral("À corriger")) return QColor("#e08f8f");
    if (t == QStringLiteral("En cours") || t == QStringLiteral("Départ")) return QColor("#e6c27a");
    return statusColor(t);
}

class PrettyDelegate : public QStyledItemDelegate {
public:
    PrettyDelegate(QTableWidget *table, TablePolish *polish) : QStyledItemDelegate(table), m_table(table), m_polish(polish) {}

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const qreal shine = m_polish->glow(index.row());
        const qreal t = std::min(m_polish->appear(index.row()), shine > 0 ? qreal(Motion::ease((1 - shine) * Motion::Glow / Motion::Slow)) : 1.0);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        if (option.state & QStyle::State_Selected) p->fillRect(option.rect, QColor("#3c3727"));
        else if (m_polish->hovered() == index.row()) p->fillRect(option.rect, QColor("#2e2d2a"));
        if ((option.state & QStyle::State_Selected) && index.column() == 0) p->fillRect(QRect(option.rect.left(), option.rect.top(), 3, option.rect.height()), QColor("#e2cf9f"));
        if (shine > 0) p->fillRect(option.rect, QColor(226, 207, 159, int(60 * shine)));
        p->setPen(QColor("#2c2c30"));
        p->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
        p->setOpacity(t);
        p->translate(6 * (1 - t), 0);
        const QTableWidgetItem *head = m_table->horizontalHeaderItem(index.column());
        const QString header = head ? head->text() : QString(), text = index.data().toString();
        const QRect r = option.rect.adjusted(12, 0, -8, 0);
        if ((header == QStringLiteral("Nom") || header == QStringLiteral("Employé")) && r.width() > 110) person(p, r, text);
        else if (header == QStringLiteral("Disponibilité") || header == QStringLiteral("État") || header == QStringLiteral("Capacité") || header == QStringLiteral("Statut")) pill(p, r, text);
        else if (header == QStringLiteral("Charge") || header == QStringLiteral("Score /100") || header == QStringLiteral("Activité")) bar(p, r, text, header == QStringLiteral("Charge"));
        else {
            const bool strong = header == QStringLiteral("ID") || header == QStringLiteral("Travail net") || header == QStringLiteral("Tâche");
            const bool extra = (header == QStringLiteral("Heures sup.")) && !text.startsWith(QStringLiteral("0 h 00")) && !text.startsWith(QStringLiteral("00:00:00")) && text != QStringLiteral("—");
            p->setPen(QColor(extra ? "#e3a06d" : strong ? "#f1e6c8" : "#d3cfc5"));
            p->drawText(r, Qt::AlignVCenter | Qt::AlignLeft, p->fontMetrics().elidedText(text, Qt::ElideRight, r.width()));
        }
        p->restore();
    }

private:
    void person(QPainter *p, const QRect &r, const QString &name) const
    {
        QString initials;
        for (const QString &part : name.split(' ', Qt::SkipEmptyParts)) if (initials.size() < 2 && part[0].isLetter()) initials += part[0].toUpper();
        const QPointF c(r.left() + 14, r.center().y() + 0.5);
        p->setPen(QPen(QColor("#6b5f43"), 1));
        p->setBrush(QColor("#3a3427"));
        p->drawEllipse(c, 14, 14);
        QFont f = p->font(), small = f;
        small.setPointSizeF(8);
        small.setBold(true);
        p->setFont(small);
        p->setPen(QColor("#e9d8a8"));
        p->drawText(QRectF(c.x() - 14, c.y() - 14, 28, 28), Qt::AlignCenter, initials);
        f.setWeight(QFont::Medium);
        p->setFont(f);
        p->setPen(QColor("#f7f4ee"));
        const QRect t = r.adjusted(38, 0, 0, 0);
        p->drawText(t, Qt::AlignVCenter | Qt::AlignLeft, p->fontMetrics().elidedText(name, Qt::ElideRight, t.width()));
    }
    void pill(QPainter *p, const QRect &r, const QString &text) const
    {
        const QColor color = pillColor(text);
        QColor back = color;
        back.setAlpha(38);
        const int w = std::min(r.width(), p->fontMetrics().horizontalAdvance(text) + 36);
        const QRect box(r.left(), r.center().y() - 12, w, 25);
        p->setPen(Qt::NoPen);
        p->setBrush(back);
        p->drawRoundedRect(box, 12, 12);
        p->setBrush(color);
        p->drawEllipse(QPointF(box.left() + 13, box.center().y() + 0.5), 3, 3);
        p->setPen(color);
        p->drawText(box.adjusted(23, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, p->fontMetrics().elidedText(text, Qt::ElideRight, w - 27));
    }
    void bar(QPainter *p, const QRect &r, const QString &text, bool load) const
    {
        const int value = std::clamp(QString(text).remove('%').trimmed().toInt(), 0, 100);
        const int w = std::clamp(r.width() - 50, 10, 150);
        const QRectF track(r.left(), r.center().y() - 3, w, 6);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor("#3b3b40"));
        p->drawRoundedRect(track, 3, 3);
        const QColor color = load ? QColor(value >= 90 ? "#e08f8f" : value >= 70 ? "#e6c27a" : "#7fc08a") : QColor(value >= 75 ? "#7fc08a" : value >= 45 ? "#e6c27a" : "#8f8c86");
        p->setBrush(color);
        p->drawRoundedRect(QRectF(track.left(), track.top(), w * value / 100.0, 6), 3, 3);
        p->setPen(QColor("#d9d5cb"));
        p->drawText(QRect(r.left() + w + 10, r.top(), 48, r.height()), Qt::AlignVCenter | Qt::AlignLeft, load || text.contains('%') ? QString::number(value) + " %" : text);
    }
    QTableWidget *m_table;
    TablePolish *m_polish;
};
}

void TablePolish::apply(QWidget *root)
{
    for (QTableWidget *t : root->findChildren<QTableWidget *>())
        if (!t->property("polished").toBool()) new TablePolish(t);
}

TablePolish::TablePolish(QTableWidget *table) : QObject(table), m_table(table)
{
    table->setProperty("polished", true);
    table->setItemDelegate(new PrettyDelegate(table, this));
    for (int c = 0; c < table->columnCount(); ++c) {
        table->setItemDelegateForColumn(c, nullptr);
        int width = table->columnWidth(c);
        const QString h = table->horizontalHeaderItem(c) ? table->horizontalHeaderItem(c)->text() : QString();
        if (h == QStringLiteral("ID")) width = 92;
        else if (h == QStringLiteral("Nom") || h == QStringLiteral("Employé")) width = 190;
        else if (h == QStringLiteral("E-mail")) width = 215;
        else if (h == QStringLiteral("État") || h == QStringLiteral("Capacité") || h == QStringLiteral("Disponibilité") || h == QStringLiteral("Statut")) width = 140;
        else if (h == QStringLiteral("Charge") || h == QStringLiteral("Score /100")) width = 130;
        else if (h == QStringLiteral("Poste")) width = 150;
        m_base.append(std::max(60, width));
    }
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setHighlightSections(false);
    table->setShowGrid(false);
    table->setAlternatingRowColors(false);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    if (table->verticalHeader()->defaultSectionSize() < 44) table->verticalHeader()->setDefaultSectionSize(46);
    table->setMouseTracking(true);
    table->viewport()->setMouseTracking(true);
    table->viewport()->installEventFilter(this);
    table->installEventFilter(this);

    m_empty = new QLabel(table->viewport());
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_empty->setText(QStringLiteral("<div style='font-size:40pt;color:transparent'>.</div><div style='font-size:11pt;color:#b9b4a8'>Rien à afficher pour le moment</div>"
                                    "<div style='font-size:9pt;color:#77746e'>Les données apparaîtront ici automatiquement.</div>"));
    m_empty->setStyleSheet("background:transparent");
    m_emptyIcon = new LivingIcon(LivingIcon::Empty, 78, m_empty);
    connect(table->model(), &QAbstractItemModel::rowsInserted, this, &TablePolish::updateEmpty);
    connect(table->model(), &QAbstractItemModel::rowsRemoved, this, &TablePolish::updateEmpty);
    connect(table->model(), &QAbstractItemModel::modelReset, this, &TablePolish::updateEmpty);
    m_glow.setDuration(Motion::Glow);
    m_glow.setStartValue(1.0);
    m_glow.setEndValue(0.0);
    connect(&m_glow, &QVariantAnimation::valueChanged, this, [this] { if (alive()) m_table->viewport()->update(); });
    m_animation.setDuration(Motion::Slow);
    m_animation.setStartValue(0.0);
    m_animation.setEndValue(1.0);
    connect(&m_animation, &QVariantAnimation::valueChanged, this, [this] { if (alive()) m_table->viewport()->update(); });
    fit();
    updateEmpty();
}

qreal TablePolish::appear(int row) const
{
    if (m_animation.state() != QAbstractAnimation::Running) return 1;
    const int top = std::max(0, m_table->rowAt(0));
    const qreal t = std::clamp((m_animation.currentValue().toReal() - 0.045 * std::min(12, std::max(0, row - top))) / 0.46, 0.0, 1.0);
    return 1 - (1 - t) * (1 - t) * (1 - t);
}

void TablePolish::flash(QTableWidget *table, int row)
{
    auto *polish = table->findChild<TablePolish *>(QString(), Qt::FindDirectChildrenOnly);
    if (!polish || row < 0 || row >= table->rowCount()) return;
    polish->m_glow.stop();
    polish->m_glowRow = row;
    polish->m_glow.start();
}

qreal TablePolish::glow(int row) const
{
    return row == m_glowRow && m_glow.state() == QAbstractAnimation::Running ? m_glow.currentValue().toReal() : 0;
}

void TablePolish::fit()
{
    if (!alive()) return;
    int total = 0;
    for (int c = 0; c < m_base.size(); ++c) if (!m_table->isColumnHidden(c)) total += m_base[c];
    const int width = m_table->viewport()->width();
    if (!total || width < 50) return;
    const double scale = std::max(0.88, width / double(total));
    int used = 0, last = -1;
    for (int c = 0; c < m_base.size(); ++c) {
        if (m_table->isColumnHidden(c)) continue;
        m_table->setColumnWidth(c, int(m_base[c] * scale));
        used += m_table->columnWidth(c);
        last = c;
    }
    if (last >= 0 && used < width) m_table->setColumnWidth(last, m_table->columnWidth(last) + width - used);
}

bool TablePolish::alive() const { return m_empty && qobject_cast<QTableWidget *>(parent()); }

void TablePolish::updateEmpty()
{
    if (!alive()) return;
    m_empty->setGeometry(m_table->viewport()->rect());
    m_emptyIcon->move(m_empty->width() / 2 - 39, m_empty->height() / 2 - 88);
    m_empty->setVisible(m_table->rowCount() == 0);
}

bool TablePolish::eventFilter(QObject *object, QEvent *event)
{
    if (!alive()) return false;
    if (object == m_table && event->type() == QEvent::Show) {
        m_animation.stop();
        m_animation.start();
    } else if (object == m_table->viewport()) {
        if (event->type() == QEvent::Resize) {
            fit();
            updateEmpty();
        } else if (event->type() == QEvent::MouseMove || event->type() == QEvent::Leave) {
            const int row = event->type() == QEvent::Leave ? -1 : m_table->rowAt(int(static_cast<QMouseEvent *>(event)->position().y()));
            if (row != m_hovered) {
                m_hovered = row;
                m_table->viewport()->update();
            }
        }
    }
    return QObject::eventFilter(object, event);
}
