#include "FormAnimation.h"
#include "AnimationSpeed.h"
#include "pdfexport.h"
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>

FieldGlow::FieldGlow(QWidget *field, std::function<bool()> filled)
    : QWidget(field->parentWidget()), m_field(field), m_filled(std::move(filled))
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    field->installEventFilter(this);
    place();
    show();
    connect(&m_timer, &QTimer::timeout, this, [this] {
        m_focus += (m_focusTo - m_focus) * 0.16;
        m_check += (m_checkTo - m_check) * 0.16;
        if (std::abs(m_focusTo - m_focus) < 0.005)
            m_focus = m_focusTo;
        if (std::abs(m_checkTo - m_check) < 0.005)
            m_check = m_checkTo;
        if (m_focus == m_focusTo && m_check == m_checkTo)
            m_timer.stop();
        update();
    });
}

void FieldGlow::place()
{
    setGeometry(m_field->geometry().adjusted(-5, -5, 5, 5));
    raise();
}

void FieldGlow::refresh()
{
    m_checkTo = m_filled() ? 1 : 0;
    m_timer.start(Motion::frame(this));
}

bool FieldGlow::eventFilter(QObject *, QEvent *event)
{
    switch (event->type()) {
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
        place();
        break;
    case QEvent::FocusIn:
    case QEvent::FocusOut:
        m_focusTo = event->type() == QEvent::FocusIn ? 1 : 0;
        m_timer.start(Motion::frame(this));
        break;
    default:
        break;
    }
    return false;
}

void FieldGlow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(5, 5, -5, -5);
    if (m_focus > 0.01) {
        QPainterPath path;
        path.addRoundedRect(r, 9, 9);
        for (int i = 3; i > 0; --i) {
            QColor halo(226, 207, 159);
            halo.setAlphaF(0.07 * m_focus);
            p.setPen(QPen(halo, i * 2.6));
            p.drawPath(path);
        }
        const double length = path.length() / 1.6;
        QPen pen(QColor("#e2cf9f"), 1.6);
        pen.setDashPattern({std::max(0.01, length * m_focus), length * 2});
        p.setPen(pen);
        p.drawPath(path);
    }
    if (m_check > 0.01) {
        const QPointF c(r.right() - (m_field->inherits("QLineEdit") ? 16 : 36), r.center().y());
        const double s = Motion::spring(m_check);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#232326"));
        p.drawEllipse(c, 9 * s, 9 * s);
        p.setBrush(QColor("#7fc08a"));
        p.drawEllipse(c, 7 * s, 7 * s);
        QPainterPath mark(c + QPointF(-3, 0) * s);
        mark.lineTo(c + QPointF(-0.8, 2.4) * s);
        mark.lineTo(c + QPointF(3.2, -2.4) * s);
        p.setPen(QPen(QColor("#15201a"), 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(mark);
    }
}

FormAnimation::FormAnimation(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(168);
    m_life.start();
    m_pop.start();
    connect(&m_timer, &QTimer::timeout, this, [this] {
        m_progress += (m_progressTo - m_progress) * 0.10;
        m_level += (m_levelTo - m_level) * 0.12;
        m_value += (m_valueTo - m_value) * 0.14;
        if (isVisible())
            update();
    });
    m_timer.start(Motion::frame(this));
}

void FormAnimation::show(const Material &data)
{
    if (data.type() != m_data.type() || m_icon.isNull()) {
        QString key = data.type().toLower();
        if (!QStringList{"tissu", "dentelle", "fil", "bouton", "fermeture", "doublure", "ruban"}.contains(key))
            key = "autre";
        m_icon = QPixmap(":/assets/icons/" + key + ".png");
        m_pop.start();
    }
    m_data = data;
    int done = 0;
    for (FieldGlow *f : m_fields) {
        f->refresh();
        done += f->filled();
    }
    m_progressTo = m_fields.isEmpty() ? 0 : done / double(m_fields.size());
    m_levelTo = data.minimum() > 0    ? std::min(1.0, data.quantity() / (2 * data.minimum()))
                : data.quantity() > 0 ? 1
                                      : 0;
    m_valueTo = data.quantity() * data.price();
}

void FormAnimation::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    const double t = m_life.elapsed() / 1000.0;
    const double x0 = 6, x1 = width() - 74, y = 18, head = x0 + (x1 - x0) * m_progress;
    QPen guide(QColor("#3a3a3f"), 1.6, Qt::CustomDashLine, Qt::RoundCap);
    guide.setDashPattern({4, 4});
    p.setPen(guide);
    p.drawLine(QPointF(x0, y), QPointF(x1, y));
    QPainterPath thread(QPointF(x0, y));
    for (double x = x0; x <= head; x += 4)
        thread.lineTo(x, y + 2.4 * std::sin(x / 11 - t * 1.6));
    const bool complete = m_progress > 0.985;
    p.setPen(QPen(QColor(complete ? "#7fc08a" : "#e2cf9f"), 2.2, Qt::SolidLine, Qt::RoundCap));
    p.drawPath(thread);
    p.setPen(QPen(QColor("#f7f0dc"), 2.4, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(head, y), QPointF(head + 20, y - 5));
    p.setPen(QColor(complete ? "#7fc08a" : "#b9b4a8"));
    p.drawText(QRectF(x1 + 4, y - 12, 70, 24), Qt::AlignRight | Qt::AlignVCenter,
               QString::number(std::lround(m_progress * 100)) + " %");

    const QRectF card(1, 42, width() - 2, height() - 44);
    QLinearGradient back(card.topLeft(), card.bottomRight());
    back.setColorAt(0, QColor("#2b2924"));
    back.setColorAt(1, QColor("#1f1f22"));
    p.setPen(QPen(QColor(complete ? "#7fc08a" : "#4a4434"), 1.2));
    p.setBrush(back);
    p.drawRoundedRect(card, 12, 12);

    const QPointF c(card.left() + 62, card.center().y());
    const double pop = Motion::spring(Motion::part(m_pop.elapsed(), 0, 520));
    QRadialGradient glow(c, 52);
    glow.setColorAt(0, QColor(226, 207, 159, 46));
    glow.setColorAt(1, QColor(226, 207, 159, 0));
    p.fillRect(QRectF(c - QPointF(52, 52), QSizeF(104, 104)), glow);
    p.setPen(QPen(QColor("#6b5f43"), 1.2));
    p.setBrush(QColor("#242427"));
    p.drawEllipse(c, 38, 38);
    p.save();
    p.translate(c);
    p.rotate(14 * (1 - pop) + 2 * std::sin(t * 1.4));
    p.scale(pop, pop);
    p.drawPixmap(QRectF(-24, -24, 48, 48), m_icon, QRectF(m_icon.rect()));
    p.restore();

    QFont title("Cormorant Garamond");
    title.setPixelSize(26);
    QFont normal = p.font();
    p.setFont(title);
    p.setPen(QColor(m_data.name().isEmpty() ? "#77746d" : "#e9d8a8"));
    const double left = card.left() + 122, middle = card.left() + card.width() * 0.56;
    p.drawText(QRectF(left, card.top() + 18, middle - left - 10, 34), Qt::AlignVCenter,
               p.fontMetrics().elidedText(m_data.name().isEmpty() ? QStringLiteral("Nouvelle matière") : m_data.name(),
                                          Qt::ElideRight, int(middle - left - 10)));
    p.setFont(normal);
    p.setPen(QColor("#b9b4a8"));
    const QStringList details =
        QStringList{m_data.type(), m_data.color(), m_data.supplier()}.filter(QRegularExpression("\\S"));
    p.drawText(
        QRectF(left, card.top() + 54, middle - left - 10, 22), Qt::AlignVCenter,
        p.fontMetrics().elidedText(details.join(QStringLiteral("  ·  ")), Qt::ElideRight, int(middle - left - 10)));
    p.setPen(QColor("#8f8c86"));
    p.drawText(QRectF(left, card.top() + 78, middle - left - 10, 22), Qt::AlignVCenter,
               m_data.lot().isEmpty() ? QStringLiteral("Lot à renseigner") : QStringLiteral("Lot ") + m_data.lot());

    const bool low = m_data.quantity() < m_data.minimum();
    const QColor tone(m_data.quantity() == 0 ? "#e8a9b0" : low ? "#e6ce89" : "#b7ccb3");
    const QRectF track(middle, card.top() + 48, card.right() - 24 - middle, 9);
    p.setPen(QColor("#b9b4a8"));
    p.drawText(
        QRectF(track.left(), card.top() + 18, track.width(), 24), Qt::AlignVCenter | Qt::AlignLeft,
        QStringLiteral("Stock %1 %2  /  seuil %3")
            .arg(StockReports::quantity(m_data.quantity()), m_data.unit(), StockReports::quantity(m_data.minimum())));
    p.setPen(tone);
    p.drawText(QRectF(track.left(), card.top() + 18, track.width(), 24), Qt::AlignVCenter | Qt::AlignRight,
               m_data.quantity() == 0 ? QStringLiteral("Rupture")
               : low                  ? QStringLiteral("À commander")
                                      : QStringLiteral("Disponible"));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#38383c"));
    p.drawRoundedRect(track, 4.5, 4.5);
    p.setBrush(tone);
    if (m_level > 0.01)
        p.drawRoundedRect(QRectF(track.left(), track.top(), track.width() * m_level, 9), 4.5, 4.5);
    p.setPen(QPen(QColor("#f1e1b4"), 1.4));
    p.drawLine(QPointF(track.center().x(), track.top() - 4), QPointF(track.center().x(), track.bottom() + 4));
    p.setPen(QColor("#8f8c86"));
    p.drawText(QRectF(track.left(), card.top() + 74, track.width(), 24), Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("Valeur du stock"));
    QFont strong = normal;
    strong.setBold(true);
    strong.setPointSizeF(13);
    p.setFont(strong);
    p.setPen(QColor("#f1e1b4"));
    p.drawText(QRectF(track.left(), card.top() + 72, track.width(), 28), Qt::AlignVCenter | Qt::AlignRight,
               StockReports::money(std::abs(m_valueTo - m_value) < 0.01 ? m_valueTo : m_value));
}
