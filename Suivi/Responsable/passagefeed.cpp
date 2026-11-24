#include "passagefeed.h"
#include "UiAnimations.h"
#include "icons.h"
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace {
QColor kindColor(const QString &kind)
{
    return QColor(kind == "ARRIVEE" || kind == "RETOUR" ? "#7fc08a"
            : kind == "REFUSE"                          ? "#e08f8f"
            : kind == "PAUSE"                           ? "#e6c27a"
                                                        : "#c9b27c");
}

QString kindText(const AccessEntry &e)
{
    const bool office = e.via == "BADGE";
    if (e.kind == "ARRIVEE") {
        return office ? QStringLiteral("entrée par badge") : QStringLiteral("connexion par identifiant");
    }
    if (e.kind == "DEPART") {
        return office ? QStringLiteral("sortie par badge") : QStringLiteral("déconnexion");
    }
    if (e.kind == "PAUSE") {
        return QStringLiteral("sortie en pause");
    }
    if (e.kind == "RETOUR") {
        return QStringLiteral("retour de pause");
    }
    return office ? QStringLiteral("badge refusé à l'entrée") : QStringLiteral("connexion refusée");
}

QString kindShort(const QString &kind)
{
    if (kind == "ARRIVEE") {
        return QStringLiteral("Arrivée");
    }
    if (kind == "DEPART") {
        return QStringLiteral("Départ");
    }
    if (kind == "PAUSE") {
        return QStringLiteral("Pause");
    }
    if (kind == "RETOUR") {
        return QStringLiteral("Retour");
    }
    return QStringLiteral("Refusé");
}

QString ago(const QDateTime &at)
{
    const qint64 s = at.secsTo(QDateTime::currentDateTime());
    if (s < 15) {
        return QStringLiteral("à l'instant");
    }
    if (s < 60) {
        return QStringLiteral("il y a %1 s").arg(s / 10 * 10);
    }
    if (s < 3600) {
        return QStringLiteral("il y a %1 min").arg(s / 60);
    }
    if (at.date() == QDate::currentDate()) {
        return QStringLiteral("il y a %1 h").arg(s / 3600);
    }
    return at.toString("dd/MM");
}
}

PassageList::PassageList(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        if (isVisible() && (m_shift.running() || m_shine.running() || m_expand.running())) {
            if (m_expand.running()) {
                fit();
            }
            update();
        }
    });
    timer->start(Fx::frameInterval(this));
}

void PassageList::setEntries(const QVector<AccessEntry> &entries, bool animate)
{
    m_fresh = 0;
    if (animate && !m_entries.isEmpty()) {
        while (m_fresh < entries.size() && entries[m_fresh].id != m_entries.first().id) {
            ++m_fresh;
        }
    }
    if (m_fresh >= entries.size()) {
        m_fresh = 0;
    }
    m_entries = entries;
    if (m_fresh) {
        m_shift.jump(1);
        m_shift.go(0, Motion::Slow);
        m_shine.jump(1);
        m_shine.go(0, Motion::Glow);
        QTimer::singleShot(Motion::Glow + 40, this, [this] {
            update();
        });
    }
    if (m_open && openIndex() < 0) {
        m_open = 0;
    }
    fit();
    update();
}

void PassageList::refresh()
{
    const int open = openIndex();
    if (open >= 0 && details) {
        m_day = details(m_entries[open]);
    }
    update();
}

int PassageList::openIndex() const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == m_open) {
            return i;
        }
    }
    return -1;
}

void PassageList::fit()
{
    setMinimumHeight(int(m_entries.size() * Row + 8 + (openIndex() >= 0 ? Extra * m_expand.value() : 0)));
}

int PassageList::indexAt(int y) const
{
    const int open = openIndex();
    const int extra = open < 0 ? 0 : int(Extra * m_expand.value());
    if (open >= 0 && y >= (open + 1) * Row) {
        y = std::max((open + 1) * Row - 1, y - extra);
    }
    const int index = y / Row;
    return y >= 0 && index < m_entries.size() ? index : -1;
}

void PassageList::toggle(int index)
{
    if (index < 0 || !clickable(m_entries[index])) {
        return;
    }
    const bool same = m_entries[index].id == m_open && m_expand.target() > 0;
    if (same) {
        m_expand.go(0, Motion::Base);
    } else {
        m_open = m_entries[index].id;
        if (details) {
            m_day = details(m_entries[index]);
        }
        m_expand.jump(0);
        m_expand.go(1, Motion::Slow);
    }
    QTimer::singleShot(Motion::Slow + 40, this, [this] {
        if (m_expand.value() < 0.01) {
            m_open = 0;
        }
        fit();
        update();
    });
}

void PassageList::mousePressEvent(QMouseEvent *event)
{
    toggle(indexAt(int(event->position().y())));
}

void PassageList::mouseMoveEvent(QMouseEvent *event)
{
    const int index = indexAt(int(event->position().y()));
    setCursor(index >= 0 && clickable(m_entries[index]) ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void PassageList::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_entries.isEmpty()) {
        p.setPen(QColor("#77746e"));
        p.drawText(rect(), Qt::AlignCenter, emptyText);
        return;
    }
    const double shift = m_shift.value(), grow = m_expand.value();
    const int open = openIndex();
    p.setPen(QPen(QColor("#34343a"), 1.4));
    p.drawLine(QPointF(39, 26), QPointF(39, m_entries.size() * Row - 30 + (open >= 0 ? Extra * grow : 0)));
    for (int i = 0; i < m_entries.size(); ++i) {
        const double y =
            i * Row - (i < m_fresh ? 14 : m_fresh * Row) * shift + (open >= 0 && i > open ? Extra * grow : 0);
        const QRectF row(4, y + 3, width() - 12, Row - 6);
        if (i == open && grow > 0.01) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(226, 207, 159, int(16 * grow)));
            p.drawRoundedRect(row.adjusted(0, 0, 0, Extra * grow), 12, 12);
            p.save();
            p.setClipRect(QRectF(row.left(), row.bottom(), row.width(), Extra * grow));
            paintDetails(p, QRectF(row.left() + 68, row.bottom() - 2, row.width() - 84, Extra),
                std::clamp(grow * 1.6 - 0.6, 0.0, 1.0));
            p.restore();
        }
        if (row.bottom() < event->rect().top() || row.top() > event->rect().bottom()) {
            continue;
        }
        paintRow(p, m_entries[i], row, i < m_fresh ? 1 - shift : 1, i < m_fresh ? m_shine.value() : 0);
    }
}

void PassageList::paintRow(QPainter &p, const AccessEntry &e, const QRectF &row, double alpha, double shine) const
{
    const QColor color = kindColor(e.kind);
    const bool refused = e.kind == "REFUSE";
    p.save();
    p.setOpacity(alpha);
    if (refused || shine > 0.01) {
        QColor back = color;
        back.setAlphaF(refused ? 0.08 + 0.12 * shine : 0.14 * shine);
        p.setPen(Qt::NoPen);
        p.setBrush(back);
        p.drawRoundedRect(row, 12, 12);
    }
    const QPointF c(row.left() + 35, row.center().y());
    p.setPen(QPen(QColor("#1f1f22"), 5));
    p.setBrush(QColor("#1f1f22"));
    p.drawEllipse(c, 20, 20);
    p.setPen(QPen(color, 1.6));
    p.setBrush(QColor(refused ? "#3a2626" : "#3a3427"));
    p.drawEllipse(c, 18, 18);
    QFont f = font();
    f.setPointSizeF(9.5);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(refused ? "#f0b9bb" : "#e9d8a8"));
    QString initials;
    if (refused) {
        initials = QStringLiteral("!");
    } else {
        for (const QString &part : e.label.split(' ', Qt::SkipEmptyParts)) {
            if (initials.size() < 2) {
                initials += part[0].toUpper();
            }
        }
    }
    p.drawText(QRectF(c.x() - 18, c.y() - 18, 36, 36), Qt::AlignCenter, initials);
    const QPointF b(c.x() + 14, c.y() + 13);
    p.setPen(QPen(QColor("#1f1f22"), 2));
    p.setBrush(QColor(e.via == "BADGE" ? "#e2cf9f" : "#8fb3c9"));
    p.drawEllipse(b, 9.5, 9.5);
    p.drawPixmap(QPointF(b.x() - 6, b.y() - 6),
        Icons::pixmap(e.via == "BADGE" ? Icons::Office : Icons::Online, QColor("#1b1b1d"), 12));

    const double left = row.left() + 68, right = row.right() - 12;
    f.setPointSizeF(10);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(QColor("#ffffff"));
    const double space = right - 96 - left;
    p.drawText(QRectF(left, row.top() + 8, space, 22), Qt::AlignVCenter,
        p.fontMetrics().elidedText(e.label, Qt::ElideRight, int(space)));
    f.setPointSizeF(9);
    f.setWeight(QFont::Normal);
    p.setFont(f);
    p.setPen(QColor("#9a968d"));
    const QString line = (e.via == "BADGE" ? QStringLiteral("Bureau · ") : QStringLiteral("En ligne · ")) + kindText(e)
        + (e.detail.isEmpty() ? QString() : QStringLiteral(" · ") + e.detail);
    p.drawText(QRectF(left, row.top() + 29, space, 20), Qt::AlignVCenter,
        p.fontMetrics().elidedText(line, Qt::ElideRight, int(space)));

    const QString pill = kindShort(e.kind);
    f.setPointSizeF(8.5);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    const double w = p.fontMetrics().horizontalAdvance(pill) + 30;
    const QRectF box(right - w, row.top() + 7, w, 22);
    QColor soft = color;
    soft.setAlpha(38);
    p.setPen(Qt::NoPen);
    p.setBrush(soft);
    p.drawRoundedRect(box, 11, 11);
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPointF a(box.left() + 12, box.center().y());
    if (refused) {
        p.drawLine(a + QPointF(-3, -3), a + QPointF(3, 3));
        p.drawLine(a + QPointF(3, -3), a + QPointF(-3, 3));
    } else {
        const double way = e.kind == "ARRIVEE" || e.kind == "RETOUR" ? 1 : -1;
        p.drawLine(a + QPointF(-4 * way, 0), a + QPointF(4 * way, 0));
        p.drawLine(a + QPointF(1 * way, -3), a + QPointF(4 * way, 0));
        p.drawLine(a + QPointF(1 * way, 3), a + QPointF(4 * way, 0));
    }
    p.drawText(box.adjusted(21, 0, 0, 0), Qt::AlignVCenter, pill);
    f.setWeight(QFont::Normal);
    f.setPointSizeF(8.5);
    p.setFont(f);
    p.setPen(QColor("#77746e"));
    p.drawText(QRectF(right - 170, row.top() + 31, 170, 18), Qt::AlignRight | Qt::AlignVCenter,
        e.at.toString("HH:mm:ss") + QStringLiteral(" · ") + ago(e.at));
    p.restore();
}

void PassageList::paintDetails(QPainter &p, const QRectF &box, double alpha) const
{
    p.setOpacity(alpha);
    QFont f = font();
    const auto text = [&](const QRectF &r, const QString &t, const char *color, double size, int weight,
                          int align = Qt::AlignLeft) {
        f.setPointSizeF(size);
        f.setWeight(QFont::Weight(weight));
        p.setFont(f);
        p.setPen(QColor(color));
        p.drawText(r, align | Qt::AlignVCenter, t);
    };
    if (!m_day.login.isValid()) {
        text(box, QStringLiteral("Aucune journée enregistrée."), "#9a968d", 9, QFont::Normal);
        return;
    }
    const auto hm = [](int s) {
        return QStringLiteral("%1 h %2").arg(s / 3600).arg(s / 60 % 60, 2, 10, QChar('0'));
    };
    const int left = std::max(0, WorkRules::Day - m_day.work);
    const QString state = m_day.ended() ? QStringLiteral("Terminé") : m_day.status();
    const QStringList titles = {QStringLiteral("Arrivée"),
        m_day.ended() ? QStringLiteral("Départ") : QStringLiteral("Fin prévue"), QStringLiteral("Reste à faire"),
        QStringLiteral("Statut")};
    const QStringList values = {m_day.login.toString("HH:mm"),
        m_day.ended() ? m_day.logout.toString("HH:mm")
                      : QDateTime::currentDateTime()
                            .addSecs(left + std::max(0, WorkRules::Pause - m_day.pause))
                            .toString("HH:mm"),
        hm(left), state};
    const double w = box.width() / 4;
    for (int i = 0; i < 4; ++i) {
        text(QRectF(box.left() + i * w, box.top() + 2, w, 16), titles[i], "#8f8c86", 8, QFont::Normal);
        text(QRectF(box.left() + i * w, box.top() + 17, w, 20), values[i], "#f7f4ee", 10, QFont::DemiBold);
    }
    const auto bar = [&](double y, const QString &title, const QString &value, double ratio, const QColor &a,
                         const QColor &b) {
        text(QRectF(box.left(), y, 70, 18), title, "#b9b4a8", 8.5, QFont::Medium);
        text(QRectF(box.right() - 120, y, 120, 18), value, "#e9d8a8", 8.5, QFont::DemiBold, Qt::AlignRight);
        const QRectF track(box.left() + 64, y + 6, box.width() - 64 - 130, 6);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#3b3b40"));
        p.drawRoundedRect(track, 3, 3);
        if (ratio <= 0) {
            return;
        }
        QLinearGradient g(track.topLeft(), track.topRight());
        g.setColorAt(0, a);
        g.setColorAt(1, b);
        p.setBrush(g);
        p.drawRoundedRect(
            QRectF(track.left(), track.top(), std::max(6.0, track.width() * std::min(1.0, ratio) * alpha), 6), 3, 3);
    };
    const bool over = m_day.pause > WorkRules::Pause;
    bar(box.top() + 46, QStringLiteral("Travail"), hm(m_day.work) + QStringLiteral(" / 8 h"),
        m_day.work / double(WorkRules::Day), QColor("#4f7d58"), QColor("#7fc08a"));
    bar(box.top() + 70, QStringLiteral("Pause"), QStringLiteral("%1 / 60 min").arg(m_day.pause / 60),
        m_day.pause / double(WorkRules::Pause), QColor(over ? "#c27575" : "#b08f4f"),
        QColor(over ? "#e8a3a3" : "#e6c27a"));
}

PassageFeed::PassageFeed(ActivityStore &store, EntryFilter filter, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_filter(filter)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(10);
    auto *head = new QHBoxLayout;
    head->setSpacing(8);
    m_title = new QLabel(
        filter == EntryFilter::Refusals ? QStringLiteral("Accès refusés") : QStringLiteral("Derniers passages"));
    m_title->setProperty("section", true);
    head->addWidget(m_title);
    head->addStretch();
    auto *group = new QButtonGroup(this);
    const QStringList via = {QString(), QStringLiteral("BADGE"), QStringLiteral("LIGNE")};
    for (int i = 0; i < 3; ++i) {
        auto *b = m_filters[i] = new QPushButton(this);
        b->setCheckable(true);
        b->setChecked(i == 0);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(
            "QPushButton{padding:5px 12px;} QPushButton:checked{background:#3a3427;border:1px solid #c9b27c;color:#f1e1b4;}");
        if (i) {
            b->setIcon(QIcon(
                Icons::pixmap(i == 1 ? Icons::Office : Icons::Online, QColor(i == 1 ? "#e2cf9f" : "#8fb3c9"), 16)));
            b->setIconSize(QSize(16, 16));
        }
        group->addButton(b);
        if (filter == EntryFilter::Refusals) {
            b->hide();
        } else {
            head->addWidget(b);
        }
        connect(b, &QPushButton::clicked, this, [this, via, i] {
            m_via = via[i];
            m_last = -1;
            reload();
        });
    }
    root->addLayout(head);
    auto *frame = new QFrame;
    frame->setProperty("card", true);
    auto *box = new QVBoxLayout(frame);
    box->setContentsMargins(8, 10, 4, 10);
    m_list = new PassageList;
    if (filter == EntryFilter::Refusals) {
        m_list->emptyText = QStringLiteral("Aucun accès refusé.");
    }
    m_list->details = [this](const AccessEntry &e) {
        WorkSession day;
        for (const WorkSession &s : m_store.sessions(e.at.date())) {
            if (s.name != e.who) {
                continue;
            }
            if (!day.id) {
                day = s;
                continue;
            }
            day.work += s.work;
            day.pause += s.pause;
            day.idle += s.idle;
            day.login = std::min(day.login, s.login);
        }
        return day;
    };
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(m_list);
    scroll->setStyleSheet("QScrollArea,QScrollArea>QWidget>QWidget{background:transparent;}");
    box->addWidget(scroll);
    root->addWidget(frame, 1);
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        if (isVisible()) {
            reload();
        }
    });
    timer->start(1000);
    reload();
}

void PassageFeed::reload()
{
    const QVector<AccessEntry> list = m_store.entries(60, m_via, m_filter);
    const int newest = list.isEmpty() ? 0 : list.first().id;
    if (newest != m_last) {
        m_list->setEntries(list, m_last > 0);
    }
    m_list->refresh();
    m_last = newest;
    if (m_filter == EntryFilter::Passages) {
        countPresent();
    }
}

void PassageFeed::countPresent()
{
    QSet<QString> seen;
    int office = 0, online = 0;
    for (const WorkSession &s : m_store.sessions(QDate::currentDate())) {
        if (seen.contains(s.name)) {
            continue;
        }
        seen.insert(s.name);
        if (s.ended() || s.lost()) {
            continue;
        }
        (s.via == "BADGE" ? office : online) += 1;
    }
    m_filters[0]->setText(QStringLiteral("Tous"));
    m_filters[1]->setText(QStringLiteral(" Bureau · %1").arg(office));
    m_filters[2]->setText(QStringLiteral(" En ligne · %1").arg(online));
}
