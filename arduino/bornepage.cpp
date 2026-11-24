#include "bornepage.h"
#include "ConnectionAnimation.h"
#include "sound.h"
#include "uihelpers.h"
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSignalBlocker>
#include <QTime>
#include <QVBoxLayout>
#include <algorithm>

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
    if (m_state && ms < 1600) {
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
    p.drawText(QRectF(text.left(), text.center().y(), text.width(), text.height() / 2), Qt::AlignVCenter,
               m_bottom.left(shown));
}

namespace
{
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
    m_pick = new QComboBox;
    left->addWidget(m_pick);
    connect(m_pick, &QComboBox::activated, this, [this] {
        const int id = m_pick->currentData().toInt();
        if (!id)
            return;
        home();
        for (const QChar c : QString::number(id))
            key(c);
    });
    m_lcd = new LcdScreen;
    left->addWidget(m_lcd);
    m_step = new QLabel;
    m_step->setWordWrap(true);
    m_step->setMinimumHeight(42);
    m_step->setStyleSheet("color:#e9d8a8;font-weight:600;background:transparent;");
    left->addWidget(m_step);
    auto *grid = new QGridLayout;
    grid->setSpacing(8);
    const QString keys = "123A456B789C*0#D";
    for (int i = 0; i < 16; ++i) {
        const QChar k = keys[i];
        const QString label = k == 'A'   ? QStringLiteral("A  Entrée +")
                              : k == 'B' ? QStringLiteral("B  Sortie −")
                              : k == 'C' ? QStringLiteral("C  Voir")
                              : k == 'D' ? QStringLiteral("D  Annuler")
                              : k == '#' ? QStringLiteral("#  Valider")
                              : k == '*' ? QStringLiteral("*  Effacer")
                                         : QString(k);
        auto *b = new QPushButton(label);
        b->setObjectName(QString("key_%1").arg(k.unicode()));
        b->setMinimumHeight(44);
        b->setProperty("primary", k == 'A' || k == 'B' || k == 'C' || k == '#');
        connect(b, &QPushButton::clicked, this, [this, k] { key(k); });
        grid->addWidget(b, i / 4, i % 4);
    }
    left->addLayout(grid);
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
    m_feed->setHorizontalHeaderLabels(
        {QStringLiteral("Heure"), QStringLiteral("Source"), QStringLiteral("Message"), QStringLiteral("Réponse")});
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

void BornePage::setMaterials(const QVector<Material> &materials)
{
    const QSignalBlocker block(m_pick);
    const int keep = m_pick->currentData().toInt();
    m_pick->clear();
    m_pick->addItem(QStringLiteral("Choisir une matière (ou taper son ID)"), 0);
    for (const Material &m : materials)
        m_pick->addItem(QStringLiteral("ID %1  ·  %2").arg(m.id()).arg(m.name()), m.id());
    m_pick->setCurrentIndex(std::max(0, m_pick->findData(keep)));
}

void BornePage::step(const QString &text)
{
    m_step->setText(text);
}

void BornePage::home()
{
    m_id.clear();
    m_quantity.clear();
    m_last.clear();
    m_operation = QChar();
    m_lcd->display("AUREA  Stock", "ID matiere ?");
    step(QStringLiteral("1. Tapez l'ID de la matière (ou choisissez-la dans la liste)."));
}

void BornePage::key(QChar k)
{
    if (k == 'D') {
        home();
        return;
    }
    if (k == '*') {
        if (!m_operation.isNull() && m_quantity.isEmpty())
            m_operation = QChar();
        else if (!m_operation.isNull())
            m_quantity.chop(1);
        else
            m_id.chop(1);
        if (m_operation.isNull() && m_id.isEmpty()) {
            home();
            return;
        }
    } else if (k.isDigit()) {
        if (m_operation.isNull() && m_id.size() < 6)
            m_id += k;
        else if (!m_operation.isNull() && m_quantity.size() < 5)
            m_quantity += k;
    } else {
        if (m_operation.isNull() && m_id.isEmpty())
            m_id = m_last;
        if (m_id.isEmpty()) {
            m_lcd->display("Tapez d'abord", "l'ID matiere", 2);
            step(QStringLiteral("Il manque l'ID : tapez par exemple 3, puis A, B ou C."));
            return;
        }
        if (k == 'A' || k == 'B') {
            m_operation = k == 'A' ? 'E' : 'S';
            m_quantity.clear();
        } else if (k == 'C' || m_operation.isNull()) {
            send("C" + m_id);
            return;
        } else if (m_quantity.isEmpty()) {
            m_lcd->display("Quantite ?", "puis #", 2);
            step(QStringLiteral("Il manque la quantité : tapez un nombre, puis # Valider."));
            return;
        } else {
            send(QString(m_operation) + m_id + ":" + m_quantity);
            return;
        }
    }
    if (m_operation.isNull()) {
        m_lcd->display("ID : " + m_id, "A+  B-  C=voir");
        step(QStringLiteral("2. Choisissez : A pour ajouter, B pour retirer, C pour voir le stock."));
    } else {
        m_lcd->display((m_operation == 'E' ? "Entree ID " : "Sortie ID ") + m_id, "Qte : " + m_quantity + "  #=OK");
        step(m_quantity.isEmpty() ? QStringLiteral("3. Tapez la quantité à %1.")
                                        .arg(m_operation == 'E' ? QStringLiteral("ajouter") : QStringLiteral("retirer"))
                                  : QStringLiteral("4. Appuyez sur # Valider pour enregistrer dans Oracle."));
    }
}

void BornePage::send(const QString &message)
{
    const QString id = m_id;
    m_id.clear();
    m_quantity.clear();
    m_operation = QChar();
    if (!ask)
        return;
    const QByteArray reply = ask(message);
    exchange(QStringLiteral("Virtuelle"), message, reply);
    const bool ok = !reply.trimmed().endsWith("|2");
    const bool missing = reply.startsWith("ID introuvable");
    m_last = missing ? QString() : id;
    step(missing ? QStringLiteral("Cet ID n'existe pas. Tapez-en un autre.")
         : ok    ? QStringLiteral("%1  Continuez avec A, B ou C sur la même matière, ou tapez un autre ID.")
                    .arg(message[0] == 'C' ? QStringLiteral("Stock affiché.")
                                           : QStringLiteral("Enregistré dans Oracle."))
              : QStringLiteral("Opération refusée (voir l'écran). Recommencez avec A ou B."));
}

void BornePage::exchange(const QString &source, const QString &request, const QByteArray &reply)
{
    const QStringList parts = QString::fromLatin1(reply).trimmed().split('|');
    const int state = parts.value(2).toInt();
    m_lcd->display(parts.value(0), parts.value(1), state);
    m_flow->send(state);
    Sound::play(state == 1 ? "alerte" : state == 2 ? "refus" : "valide");
    m_feed->insertRow(0);
    const QStringList values = {QTime::currentTime().toString("HH:mm:ss"), source, request,
                                parts.value(0) + (parts.value(1).isEmpty() ? QString() : " · " + parts.value(1))};
    for (int c = 0; c < 4; ++c) {
        m_feed->setItem(0, c, cell(values[c]));
        m_feed->item(0, c)->setForeground(QColor(state == 1 ? "#e6ce89" : state == 2 ? "#e8a9b0" : "#d3cfc5"));
    }
    while (m_feed->rowCount() > 30)
        m_feed->removeRow(30);
}
