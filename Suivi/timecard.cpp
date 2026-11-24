#include "timecard.h"
#include "UiAnimations.h"
#include "activitystore.h"
#include "icons.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace {
QString hourMinute(int seconds) { return QStringLiteral("%1 h %2").arg(seconds / 3600).arg(seconds / 60 % 60, 2, 10, QChar('0')); }
}

TimeCard::TimeCard(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(420, 138);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAccessibleName(QStringLiteral("Temps de travail et pause"));
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this] {
        if (!m_flip.running()) m_timer->stop();
        update();
    });
}

void TimeCard::setTimes(int work, int pause, bool paused, bool running)
{
    if (work == m_work && pause == m_pause && paused == m_paused && running == m_running) return;
    m_work = work;
    m_pause = pause;
    m_paused = paused;
    m_running = running;
    update();
}

void TimeCard::setBack(bool back)
{
    if ((m_flip.target() > 0.5) == back) return;
    if (!isVisible()) m_flip.jump(back);
    else {
        m_flip.go(back, Motion::Slow);
        m_timer->start(Fx::frameInterval(this));
    }
    setToolTip(back ? QStringLiteral("Cliquez pour voir le temps restant") : QStringLiteral("Cliquez pour voir le temps passé"));
    update();
}

void TimeCard::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()) && clicked) clicked();
}

void TimeCard::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Space || event->key() == Qt::Key_Return) && clicked) clicked();
    else QWidget::keyPressEvent(event);
}

void TimeCard::enterEvent(QEnterEvent *) { m_hover = true; update(); }
void TimeCard::leaveEvent(QEvent *) { m_hover = false; update(); }

void TimeCard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double t = m_flip.value(), scale = std::max(0.02, std::abs(std::cos(t * 3.14159265)));
    const bool back = t > 0.5;
    p.translate(width() / 2.0, 0);
    p.scale(scale, 1);
    p.translate(-width() / 2.0, 0);

    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(QColor(m_hover || hasFocus() ? "#c9b27c" : "#323236"), 1));
    p.setBrush(QColor("#232326"));
    p.drawRoundedRect(box, 12, 12);

    const int workLeft = WorkRules::Day - m_work, pauseLeft = WorkRules::Pause - m_pause;
    const bool extra = workLeft < 0, over = pauseLeft < 0;
    const double workRatio = m_work / double(WorkRules::Day), pauseRatio = m_pause / double(WorkRules::Pause);
    const QString target = QStringLiteral(" sur ") + hourMinute(WorkRules::Day);
    Half work, pause;
    if (back) {
        work = {QStringLiteral("Temps travaillé"), clockText(m_work), QStringLiteral("%1 %").arg(int(workRatio * 100)) + target, workRatio, false};
        pause = {QStringLiteral("Pause prise"), clockText(m_pause), over ? QStringLiteral("dépassée de %1").arg(clockText(-pauseLeft)) : QStringLiteral("sur 60 min autorisées"), pauseRatio, over};
    } else {
        work = {extra ? QStringLiteral("Heures supplémentaires") : QStringLiteral("Travail restant"), clockText(std::abs(workLeft)),
                extra ? QStringLiteral("journée de 8 h terminée") : QStringLiteral("fin de la journée de 8 h"), extra ? 1 : 1 - workRatio, false};
        if (!extra) work.note = m_paused ? QStringLiteral("chronomètre en pause") : QStringLiteral("fin prévue à %1").arg(QDateTime::currentDateTime().addSecs(workLeft + std::max(0, pauseLeft)).toString("HH:mm"));
        pause = {over ? QStringLiteral("Pause dépassée") : QStringLiteral("Pause restante"), clockText(std::abs(pauseLeft)),
                 m_paused ? QStringLiteral("pause en cours") : m_pause ? QStringLiteral("%1 min déjà prises").arg(m_pause / 60) : QStringLiteral("pas encore prise"), over ? 1 : 1 - pauseRatio, over};
    }
    const double middle = std::round(box.left() + box.width() * 0.55);
    p.setPen(QPen(QColor("#323236"), 1));
    p.drawLine(QPointF(middle, box.top() + 20), QPointF(middle, box.bottom() - 20));
    paintHalf(p, QRectF(box.left() + 20, box.top(), middle - box.left() - 40, box.height()), work, false, m_running);
    paintHalf(p, QRectF(middle + 20, box.top(), box.right() - middle - 40, box.height()), pause, true, m_paused);

    const QPointF c(box.right() - 20, box.top() + 22);
    p.setPen(QPen(QColor(m_hover ? "#e2cf9f" : "#6f6c66"), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (double way : {1.0, -1.0}) {
        p.drawLine(c + QPointF(-5, -3 * way), c + QPointF(5, -3 * way));
        p.drawLine(c + QPointF(5 * way, -3 * way), c + QPointF(2 * way, -3 * way - 3));
    }
}

void TimeCard::paintHalf(QPainter &p, const QRectF &box, const Half &half, bool pause, bool live) const
{
    const QColor accent(half.alert ? "#e08f8f" : pause ? "#e6c27a" : "#7fc08a");
    QFont f = font();
    p.setPen(Qt::NoPen);
    QColor soft = accent;
    soft.setAlpha(34);
    p.setBrush(soft);
    const QRectF icon(box.left(), box.top() + 16, 26, 26);
    p.drawRoundedRect(icon, 8, 8);
    if (pause) {
        p.setBrush(accent);
        p.drawRoundedRect(QRectF(icon.center().x() - 5, icon.center().y() - 5, 3.4, 10), 1.5, 1.5);
        p.drawRoundedRect(QRectF(icon.center().x() + 1.6, icon.center().y() - 5, 3.4, 10), 1.5, 1.5);
    } else p.drawPixmap(QPointF(icon.center().x() - 8, icon.center().y() - 8), Icons::pixmap(Icons::Clock, accent, 16));
    f.setPointSizeF(9);
    f.setWeight(QFont::Medium);
    p.setFont(f);
    p.setPen(QColor("#b9b4a8"));
    p.drawText(QRectF(icon.right() + 10, icon.top(), box.width() - 36, 26), Qt::AlignVCenter, half.title);
    if (live) {
        p.setPen(Qt::NoPen);
        p.setBrush(accent);
        p.drawEllipse(QPointF(icon.right() + 22 + p.fontMetrics().horizontalAdvance(half.title), icon.center().y()), 3.2, 3.2);
    }
    f.setPointSizeF(21);
    f.setWeight(QFont::Bold);
    p.setFont(f);
    p.setPen(half.alert ? accent : QColor("#ffffff"));
    p.drawText(QRectF(box.left(), box.top() + 46, box.width(), 38), Qt::AlignVCenter, half.value);

    const QRectF track(box.left(), box.bottom() - 46, box.width(), 6);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#3b3b40"));
    p.drawRoundedRect(track, 3, 3);
    const double ratio = std::clamp(half.ratio, 0.0, 1.0);
    if (ratio > 0) {
        QLinearGradient g(track.topLeft(), track.topRight());
        g.setColorAt(0, accent.darker(150));
        g.setColorAt(1, accent);
        p.setBrush(g);
        p.drawRoundedRect(QRectF(track.left(), track.top(), std::max(6.0, track.width() * ratio), 6), 3, 3);
    }
    f.setPointSizeF(9);
    f.setWeight(QFont::Normal);
    p.setFont(f);
    p.setPen(half.alert ? accent : QColor("#8f8c86"));
    p.drawText(QRectF(box.left(), track.bottom() + 8, box.width(), 20), Qt::AlignVCenter, p.fontMetrics().elidedText(half.note, Qt::ElideRight, int(box.width())));
}

ActivityLog::ActivityLog(QWidget *parent) : QWidget(parent)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this] {
        if (!m_enter.running()) m_timer->stop();
        update();
    });
    fit();
}

ActivityLog::Kind ActivityLog::kindOf(const QString &text)
{
    if (text.startsWith("PC ") || text.startsWith("Inactif")) return Idle;
    if (text.startsWith(QStringLiteral("Début de pause")) || text.startsWith(QStringLiteral("Pause"))) return Pause;
    if (text.startsWith("Connexion") || text.startsWith("Reprise") || text.startsWith("Retour") || text.startsWith(QStringLiteral("Déconnexion"))) return Presence;
    return Task;
}

QString ActivityLog::kindName(Kind kind)
{
    return kind == Presence ? QStringLiteral("Présence") : kind == Pause ? QStringLiteral("Pause") : kind == Task ? QStringLiteral("Tâche") : QStringLiteral("Inactivité");
}

QColor ActivityLog::kindColor(Kind kind) { return QColor(kind == Presence ? "#7fc08a" : kind == Pause ? "#e6c27a" : kind == Task ? "#8fb3c9" : "#e3a06d"); }

void ActivityLog::add(const QDateTime &at, const QString &text)
{
    m_rows.prepend({at, text, kindOf(text)});
    if (isVisible() && (m_filter < 0 || m_filter == m_rows.first().kind)) {
        m_enter.jump(1);
        m_enter.go(0, Motion::Slow);
        m_timer->start(Fx::frameInterval(this));
    }
    fit();
}

void ActivityLog::setFilter(int kind)
{
    m_filter = kind;
    m_enter.jump(0);
    fit();
}

int ActivityLog::count(int kind) const
{
    return int(std::count_if(m_rows.begin(), m_rows.end(), [kind](const Row &r) { return kind < 0 || r.kind == kind; }));
}

void ActivityLog::fit()
{
    setMinimumHeight(std::max(90, count(m_filter) * RowHeight + 8));
    update();
}

void ActivityLog::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int total = count(m_filter);
    if (!total) {
        p.setPen(QColor("#77746e"));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("Aucun événement de ce type."));
        return;
    }
    const double shift = m_enter.value();
    p.setPen(QPen(QColor("#34343a"), 1.4));
    p.drawLine(QPointF(34, RowHeight / 2.0), QPointF(34, (total - 0.5) * RowHeight));
    int i = 0;
    for (const Row &r : m_rows) {
        if (m_filter >= 0 && r.kind != m_filter) continue;
        const int index = i++;
        const QRectF row(6, index * RowHeight + 3 - (index ? RowHeight : 14) * shift, width() - 16, RowHeight - 6);
        if (!row.intersects(QRectF(event->rect()))) continue;
        p.setOpacity(index ? 1 : 1 - shift);
        const QColor color = kindColor(r.kind);
        QColor soft = color;
        soft.setAlpha(34);
        const QPointF c(34, row.center().y());
        p.setPen(QPen(color, 1.6));
        p.setBrush(QColor("#232326"));
        p.drawEllipse(c, 16, 16);
        p.setPen(Qt::NoPen);
        p.setBrush(soft);
        p.drawEllipse(c, 16, 16);
        if (r.kind == Pause) {
            p.setBrush(color);
            p.drawRoundedRect(QRectF(c.x() - 4.6, c.y() - 5, 3.2, 10), 1.5, 1.5);
            p.drawRoundedRect(QRectF(c.x() + 1.4, c.y() - 5, 3.2, 10), 1.5, 1.5);
        } else p.drawPixmap(QPointF(c.x() - 8, c.y() - 8), Icons::pixmap(r.kind == Presence ? Icons::Badge : r.kind == Task ? Icons::Check : Icons::Alert, color, 16));

        QFont f = font();
        f.setPointSizeF(8.5);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        const QString name = kindName(r.kind);
        const double w = p.fontMetrics().horizontalAdvance(name) + 24;
        const QRectF pill(row.right() - w - 6, row.center().y() - 11, w, 22);
        p.setPen(Qt::NoPen);
        p.setBrush(soft);
        p.drawRoundedRect(pill, 11, 11);
        p.setPen(color);
        p.drawText(pill, Qt::AlignCenter, name);

        const double left = 68, space = pill.left() - left - 16;
        const int cut = int(r.text.indexOf(" : "));
        const QString head = cut > 0 ? r.text.left(cut) : r.text.section(QStringLiteral(" — "), 0, 0);
        const QString tail = cut > 0 ? r.text.mid(cut + 3) : r.text.section(QStringLiteral(" — "), 1);
        f.setPointSizeF(10);
        p.setFont(f);
        p.setPen(QColor("#f7f4ee"));
        p.drawText(QRectF(left, row.top() + 5, space, 22), Qt::AlignVCenter, p.fontMetrics().elidedText(head, Qt::ElideRight, int(space)));
        f.setPointSizeF(9);
        f.setWeight(QFont::Normal);
        p.setFont(f);
        p.setPen(QColor("#8f8c86"));
        const QString line = r.at.toString("HH:mm:ss") + (tail.isEmpty() ? QString() : QStringLiteral(" · ") + tail);
        p.drawText(QRectF(left, row.top() + 27, space, 20), Qt::AlignVCenter, p.fontMetrics().elidedText(line, Qt::ElideRight, int(space)));
    }
}
