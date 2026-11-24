#include "TableAnimation.h"
#include "AnimationSpeed.h"
#include <QEvent>
#include <QHeaderView>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QTableWidget>

TableAnimation::TableAnimation(QTableWidget *table) : QStyledItemDelegate(table), m_table(table)
{
    table->setItemDelegate(this);
    table->setShowGrid(false);
    table->setAlternatingRowColors(false);
    table->setMouseTracking(true);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->viewport()->setMouseTracking(true);
    table->viewport()->installEventFilter(this);
    table->installEventFilter(this);
    m_life.start();
    connect(table->model(), &QAbstractItemModel::rowsInserted, this, [this] {
        if (!m_enter.isValid() || m_enter.elapsed() > 120)
            m_enter.start();
        wake();
    });
    connect(&m_timer, &QTimer::timeout, this, [this] {
        bool moving = m_enter.isValid() && m_enter.elapsed() < 1200;
        for (auto it = m_glow.begin(); it != m_glow.end();) {
            const double target = it.key() == m_hover ? 1 : 0;
            it.value() += (target - it.value()) * 0.22;
            if (std::abs(target - it.value()) > 0.01)
                moving = true;
            else
                it.value() = target;
            if (it.value() == 0)
                it = m_glow.erase(it);
            else
                ++it;
        }
        m_table->viewport()->update();
        if (!moving)
            m_timer.stop();
    });
    connect(&m_pulse, &QTimer::timeout, this, [this] { if (m_table->isVisible()) m_table->viewport()->update(); });
    for (int c = 0; c < table->columnCount(); ++c)
        if (header(c) == QStringLiteral("État"))
            m_pulse.start(50);
}

void TableAnimation::wake()
{
    if (!m_timer.isActive())
        m_timer.start(Motion::frame(m_table));
}

QString TableAnimation::header(int column) const
{
    const QTableWidgetItem *item = m_table->horizontalHeaderItem(column);
    return item ? item->text() : QString();
}

double TableAnimation::glow(int row) const
{
    return m_glow.value(row, 0);
}

bool TableAnimation::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_table && event->type() == QEvent::Show) {
        m_enter.start();
        wake();
    } else if (object == m_table->viewport() && (event->type() == QEvent::MouseMove || event->type() == QEvent::Leave)) {
        const int row = event->type() == QEvent::Leave ? -1 : m_table->rowAt(int(static_cast<QMouseEvent *>(event)->position().y()));
        if (row != m_hover) {
            m_hover = row;
            if (row >= 0 && !m_glow.contains(row))
                m_glow.insert(row, 0.01);
            wake();
        }
    }
    return false;
}

QPixmap TableAnimation::icon(const QString &type) const
{
    QString key = type.toLower();
    if (!QStringList{"tissu", "dentelle", "fil", "bouton", "fermeture", "doublure", "ruban"}.contains(key))
        key = "autre";
    if (!m_icons.contains(key)) {
        QPixmap image(":/assets/icons/" + key + ".png");
        const double ratio = m_table->devicePixelRatioF();
        image = image.scaled(QSize(26, 26) * ratio, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        image.setDevicePixelRatio(ratio);
        m_icons.insert(key, image);
    }
    return m_icons[key];
}

void TableAnimation::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const int row = index.row(), column = index.column();
    const double ms = m_enter.isValid() ? m_enter.elapsed() - 45 * std::min(row, 12) : 1e9;
    const double in = Motion::soft(ms / 520), hover = glow(row);
    const bool selected = option.state & QStyle::State_Selected;
    const QString name = header(column), text = index.data().toString();
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    if (selected)
        p->fillRect(option.rect, QColor("#3c3727"));
    if (hover > 0.01)
        p->fillRect(option.rect, QColor(226, 207, 159, int(20 * hover)));
    if (column == 0 && (selected || hover > 0.01)) {
        const double h = option.rect.height() * (selected ? 1 : hover);
        p->fillRect(QRectF(option.rect.left(), option.rect.center().y() - h / 2, 3, h), QColor("#e2cf9f"));
    }
    p->setPen(QColor("#2c2c30"));
    p->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    p->setOpacity(std::clamp(in, 0.0, 1.0));
    p->translate(18 * (1 - in), 0);
    QRect r = option.rect.adjusted(10, 0, -8, 0);
    if (name == QStringLiteral("État")) {
        const QColor tone(text == QStringLiteral("Rupture") ? "#e8a9b0" : text == QStringLiteral("Disponible") ? "#b7ccb3" : "#e6ce89");
        const double beat = text == QStringLiteral("Disponible") ? 0 : 0.5 + 0.5 * std::sin(m_life.elapsed() / (text == QStringLiteral("Rupture") ? 260.0 : 520.0));
        QColor back = tone;
        back.setAlpha(int(32 + 22 * beat));
        const QRect box(r.left(), r.center().y() - 12, std::min(r.width(), p->fontMetrics().horizontalAdvance(text) + 36), 25);
        p->setPen(Qt::NoPen);
        p->setBrush(back);
        p->drawRoundedRect(box, 12, 12);
        QColor halo = tone;
        halo.setAlphaF(0.35 * beat);
        p->setBrush(halo);
        p->drawEllipse(QPointF(box.left() + 13, box.center().y() + 0.5), 3 + 3 * beat, 3 + 3 * beat);
        p->setBrush(tone);
        p->drawEllipse(QPointF(box.left() + 13, box.center().y() + 0.5), 3, 3);
        p->setPen(tone);
        p->drawText(box.adjusted(23, 0, 0, 0), Qt::AlignVCenter, text);
        p->restore();
        return;
    }
    if (name == QStringLiteral("Matière") || (name == QStringLiteral("Type") && column == 0)) {
        const int typeColumn = name == QStringLiteral("Type") ? 0 : -1;
        QString type = typeColumn == 0 ? text : QString();
        for (int c = 1; c < m_table->columnCount() && type.isEmpty(); ++c)
            if (header(c) == QStringLiteral("Type"))
                type = index.sibling(row, c).data().toString();
        if (type.isEmpty())
            type = text.section(' ', 0, 0);
        p->save();
        p->translate(r.left() + 13, r.center().y() + 0.5);
        p->rotate(-10 * hover);
        p->scale(Motion::mix(0.4, 1, in) * (1 + 0.14 * hover), Motion::mix(0.4, 1, in) * (1 + 0.14 * hover));
        p->drawPixmap(QPointF(-13, -13), icon(type));
        p->restore();
        r.setLeft(r.left() + 36);
    }
    QFont font = option.font;
    const QVariant own = index.data(Qt::FontRole);
    if (own.isValid())
        font = qvariant_cast<QFont>(own).resolve(font);
    p->setFont(font);
    const QVariant fore = index.data(Qt::ForegroundRole);
    p->setPen(fore.isValid() ? qvariant_cast<QBrush>(fore).color() : QColor("#e4e0d7"));
    const QVariant align = index.data(Qt::TextAlignmentRole);
    const int flags = align.isValid() ? align.toInt() : int(Qt::AlignLeft | Qt::AlignVCenter);
    if (name == QStringLiteral("Stock")) {
        double minimum = 0;
        const QLocale french(QLocale::French, QLocale::France);
        const auto value = [&](QString s) { s.remove(QChar(0x202F)).remove(QChar(0x00A0)).remove(' '); return french.toDouble(s); };
        for (int c = 0; c < m_table->columnCount(); ++c)
            if (header(c) == QStringLiteral("Seuil"))
                minimum = value(index.sibling(row, c).data().toString());
        const double stock = value(text), level = minimum > 0 ? std::min(1.0, stock / (2 * minimum)) : stock > 0 ? 1 : 0;
        p->drawText(r.adjusted(0, -7, 0, -7), flags, text);
        const QRectF track(r.left(), r.center().y() + 8, r.width(), 4);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor("#3b3b40"));
        p->drawRoundedRect(track, 2, 2);
        p->setBrush(QColor(stock < minimum ? (stock == 0 ? "#e8a9b0" : "#e6ce89") : "#b7ccb3"));
        const double grown = Motion::soft((ms - 200) / 800) * level;
        if (grown > 0)
            p->drawRoundedRect(QRectF(track.left(), track.top(), track.width() * grown, 4), 2, 2);
    } else {
        p->drawText(r, flags, p->fontMetrics().elidedText(text, Qt::ElideRight, r.width()));
    }
    p->restore();
}
