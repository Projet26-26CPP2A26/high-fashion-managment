#include "bornepage.h"
#include "ConnectionAnimation.h"
#include "sound.h"
#include "uihelpers.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QTime>
#include <QVBoxLayout>

LcdScreen::LcdScreen(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(300, 112);
    connect(&m_timer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
}

void LcdScreen::display(const QString &top, const QString &bottom, int state)
{
    m_top = top.left(16);
    m_bottom = bottom.left(16);
    m_state = state;
    m_clock.start();
    m_timer.start(Motion::frame(this));
    update();
}

void LcdScreen::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double ms = m_clock.isValid() ? m_clock.elapsed() : 1e9;
    if (ms > 1600)
        m_timer.stop();
    if (m_state == 2 && ms < 420)
        p.translate(std::sin(ms / 17) * 5 * (1 - ms / 420), 0);
    const QColor tone(m_state == 1 ? "#e6ce89" : m_state == 2 ? "#e08f8f" : "#b7ccb3");
    const QRectF box = QRectF(rect()).adjusted(8, 8, -8, -8);
    if (m_state && ms < 1600) {                    // alerte : trois pulsations, comme les trois bips
        QColor halo = tone;
        halo.setAlphaF(0.45 * std::pow(std::sin(ms / 1600 * 3 * 3.1416), 2));
        p.setPen(QPen(halo, 8));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(box, 12, 12);
    }
    p.setPen(QPen(QColor("#6b5f43"), 1.4));
    p.setBrush(QColor("#151517"));
    p.drawRoundedRect(box, 12, 12);
    const QRectF glass = box.adjusted(12, 11, -12, -11);
    p.setPen(QPen(tone, 1.2));
    p.setBrush(QColor("#1d2118"));
    p.drawRoundedRect(glass, 7, 7);
    QFont font("Consolas");
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(std::clamp(int(glass.width() / 10.2), 14, 30));
    p.setFont(font);
    p.setPen(QColor("#dfe8b8"));
    const int shown = std::min(16, int(ms / 22) + 1);
    const QRectF text = glass.adjusted(14, 4, -6, -4);
    p.drawText(QRectF(text.topLeft(), QSizeF(text.width(), text.height() / 2)), Qt::AlignVCenter, m_top.left(shown));
    p.drawText(QRectF(text.left(), text.center().y(), text.width(), text.height() / 2), Qt::AlignVCenter, m_bottom.left(shown));
}

namespace {
QFrame *card(QLayout *content)
{
    auto *frame = new QFrame;
    frame->setProperty("card", true);
    content->setContentsMargins(20, 18, 20, 18);
    frame->setLayout(content);
    return frame;
}
QLabel *text(const QString &value, const char *role)
{
    auto *label = new QLabel(value);
    label->setProperty(role, true);
    label->setWordWrap(true);
    return label;
}
}

BornePage::BornePage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(18);

    auto *left = new QVBoxLayout;
    left->setSpacing(12);
    left->addWidget(text(QStringLiteral("Borne virtuelle"), "section"));
    left->addWidget(text(QStringLiteral("Même clavier que la borne Arduino. Les opérations modifient réellement le stock dans Oracle."), "muted"));
    m_lcd = new LcdScreen;
    left->addWidget(m_lcd);
    auto *grid = new QGridLayout;
    grid->setSpacing(8);
    const QString keys = "123A456B789C*0#D";
    for (int i = 0; i < 16; ++i) {
        const QChar k = keys[i];
        auto *b = new QPushButton(QString(k));
        b->setObjectName(QString("key_%1").arg(k.unicode()));
        b->setMinimumHeight(44);
        if (!k.isDigit())
            b->setProperty("primary", k == 'A' || k == 'B' || k == 'C' || k == '#');
        connect(b, &QPushButton::clicked, this, [this, k] { key(k); });
        grid->addWidget(b, i / 4, i % 4);
    }
    left->addLayout(grid);
    left->addWidget(text(QStringLiteral("ID puis C : consulter     ID, A, quantité, # : entrée     ID, B, quantité, # : sortie     * ou D : annuler"), "muted"));
    left->addStretch();
    root->addWidget(card(left), 2);

    auto *right = new QVBoxLayout;
    right->setSpacing(12);
    right->addWidget(text(QStringLiteral("Liaison Arduino"), "section"));
    auto *line = new QHBoxLayout;
    m_light = new LinkLight;
    m_link = text(QStringLiteral("Arduino non connecté"), "muted");
    line->addWidget(m_light);
    line->addWidget(m_link, 1);
    right->addLayout(line);
    m_flow = new DataFlow;
    right->addWidget(m_flow);
    right->addWidget(text(QStringLiteral("Derniers échanges"), "section"));
    m_feed = new QTableWidget(0, 4);
    m_feed->setHorizontalHeaderLabels({QStringLiteral("Heure"), QStringLiteral("Source"), QStringLiteral("Message"), QStringLiteral("Réponse")});
    m_feed->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_feed->setSelectionMode(QAbstractItemView::NoSelection);
    m_feed->setFocusPolicy(Qt::NoFocus);
    configure(m_feed, {84, 96, 100, 260});
    right->addWidget(m_feed, 1);
    root->addWidget(card(right), 3);
    home();
}

void BornePage::setLink(bool on, const QString &value)
{
    m_light->setOn(on);
    m_flow->setOn(on);
    m_link->setText(value);
}

void BornePage::home()
{
    m_id.clear();
    m_quantity.clear();
    m_operation = QChar();
    m_lcd->display("AUREA  Stock", "ID matiere ?");
}

void BornePage::key(QChar k)
{
    if (k == '*' || k == 'D') {
        home();
        return;
    }
    if (k.isDigit()) {
        if (m_operation.isNull() && m_id.size() < 6)
            m_id += k;
        else if (!m_operation.isNull() && m_quantity.size() < 5)
            m_quantity += k;
    } else if (m_operation.isNull() && !m_id.isEmpty() && k == 'C') {
        send("C" + m_id);
        return;
    } else if (m_operation.isNull() && !m_id.isEmpty() && (k == 'A' || k == 'B')) {
        m_operation = k == 'A' ? 'E' : 'S';
    } else if (k == '#' && !m_operation.isNull() && !m_quantity.isEmpty()) {
        send(QString(m_operation) + m_id + ":" + m_quantity);
        return;
    }
    if (m_operation.isNull())
        m_lcd->display("ID : " + m_id, "A+  B-  C=voir");
    else
        m_lcd->display((m_operation == 'E' ? "Entree ID " : "Sortie ID ") + m_id, "Qte : " + m_quantity + "  #=OK");
}

void BornePage::send(const QString &message)
{
    m_id.clear();
    m_quantity.clear();
    m_operation = QChar();
    if (ask)
        exchange(QStringLiteral("Virtuelle"), message, ask(message));
}

void BornePage::exchange(const QString &source, const QString &request, const QByteArray &reply)
{
    const QStringList parts = QString::fromLatin1(reply).trimmed().split('|');
    const int state = parts.value(2).toInt();
    m_lcd->display(parts.value(0), parts.value(1), state);
    m_flow->send(state);
    Sound::play(state == 1 ? "alerte" : state == 2 ? "refus" : "valide");
    m_feed->insertRow(0);
    const QStringList values = {QTime::currentTime().toString("HH:mm:ss"), source, request, parts.value(0) + (parts.value(1).isEmpty() ? QString() : " · " + parts.value(1))};
    for (int c = 0; c < 4; ++c) {
        m_feed->setItem(0, c, cell(values[c]));
        m_feed->item(0, c)->setForeground(QColor(state == 1 ? "#e6ce89" : state == 2 ? "#e8a9b0" : "#d3cfc5"));
    }
    while (m_feed->rowCount() > 30)
        m_feed->removeRow(30);
}
