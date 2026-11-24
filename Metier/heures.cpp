#include "heures.h"
#include "activitystore.h"
#include "employeerepository.h"
#include "pdfexport.h"
#include "uihelpers.h"
#include <QButtonGroup>
#include <QLineEdit>
#include <QMap>
#include <QShowEvent>

using namespace UiHelpers;

HoursView::HoursView(EmployeeRepository &repository, ActivityStore &store, QWidget *parent)
    : QWidget(parent)
    , m_repository(repository)
    , m_store(store)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);

    auto *cards = new QHBoxLayout;
    cards->setSpacing(14);
    const QStringList titles = {QStringLiteral("Employés pointés"), QStringLiteral("Heures travaillées"),
        QStringLiteral("Heures supplémentaires"), QStringLiteral("Pauses")};
    for (int i = 0; i < 4; ++i) {
        auto *frame = new QFrame;
        frame->setProperty("card", true);
        auto *box = new QVBoxLayout(frame);
        box->setContentsMargins(18, 12, 18, 12);
        box->setSpacing(2);
        auto *title = new QLabel(titles[i]);
        title->setProperty("muted", true);
        m_values[i] = new QLabel(QStringLiteral("0"));
        m_values[i]->setProperty("kpi", true);
        box->addWidget(title);
        box->addWidget(m_values[i]);
        cards->addWidget(frame);
    }
    root->addLayout(cards);

    auto *filters = new QHBoxLayout;
    filters->setSpacing(10);
    m_search = new QLineEdit;
    m_search->setPlaceholderText(QStringLiteral("Rechercher un employé…"));
    m_search->setClearButtonEnabled(true);
    m_period = new QComboBox;
    m_period->addItems({QStringLiteral("Aujourd'hui"), QStringLiteral("7 derniers jours"), QStringLiteral("Ce mois"),
        QStringLiteral("Tous les jours")});
    filters->addWidget(m_search, 1);
    filters->addWidget(m_period);
    auto *group = new QButtonGroup(this);
    const QStringList modes = {QStringLiteral("Par jour"), QStringLiteral("Total par employé")};
    for (int i = 0; i < 2; ++i) {
        m_modes[i] = new QPushButton(modes[i]);
        m_modes[i]->setCheckable(true);
        m_modes[i]->setChecked(i == 0);
        m_modes[i]->setCursor(Qt::PointingHandCursor);
        m_modes[i]->setStyleSheet(
            "QPushButton:checked{background:#3a3427;border:1px solid #c9b27c;color:#f1e1b4;}");
        group->addButton(m_modes[i]);
        filters->addWidget(m_modes[i]);
        connect(m_modes[i], &QPushButton::clicked, this, [this, i] {
            m_total = i == 1;
            m_first = true;
            refresh();
        });
    }
    auto *pdf = new QPushButton(QStringLiteral("Exporter PDF"));
    pdf->setCursor(Qt::PointingHandCursor);
    filters->addWidget(pdf);
    root->addLayout(filters);

    const auto table = [this](const QStringList &columns, const QList<int> &widths) {
        auto *t = new QTableWidget(0, int(columns.size()));
        t->setHorizontalHeaderLabels(columns);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->setSelectionMode(QAbstractItemView::SingleSelection);
        t->setSelectionBehavior(QAbstractItemView::SelectRows);
        setupTable(t, widths);
        return t;
    };
    m_daily = table({QStringLiteral("Date"), QStringLiteral("Employé"), QStringLiteral("Arrivée"),
                        QStringLiteral("Départ"), QStringLiteral("Pause"), QStringLiteral("Travail net"),
                        QStringLiteral("Heures sup."), QStringLiteral("État")},
        {105, 190, 85, 85, 85, 110, 110, 120});
    m_summary = table({QStringLiteral("Employé"), QStringLiteral("Jours"), QStringLiteral("Pause"),
                          QStringLiteral("Travail net"), QStringLiteral("Moyenne / jour"),
                          QStringLiteral("Heures sup.")},
        {190, 90, 110, 130, 140, 130});
    root->addWidget(m_daily, 1);
    root->addWidget(m_summary, 1);
    m_summary->hide();
    auto *note = new QLabel(QStringLiteral(
        "Travail net = départ − arrivée − pause. Heures supplémentaires : au-delà de 8 h nettes par jour."));
    note->setProperty("muted", true);
    root->addWidget(note);

    connect(m_search, &QLineEdit::textChanged, this, [this] {
        refresh();
    });
    connect(m_period, &QComboBox::currentIndexChanged, this, [this] {
        refresh();
    });
    connect(pdf, &QPushButton::clicked, this, [this] {
        exportPdf();
    });
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        if (isVisible()) {
            refresh();
        }
    });
    timer->start(5000);
}

void HoursView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refresh();
}

bool HoursView::inPeriod(const QDate &day) const
{
    const QDate today = QDate::currentDate();
    switch (m_period->currentIndex()) {
    case 0:
        return day == today;
    case 1:
        return day > today.addDays(-7) && day <= today;
    case 2:
        return day.year() == today.year() && day.month() == today.month();
    default:
        return true;
    }
}

QVector<HoursView::Line> HoursView::lines() const
{
    const QDate today = QDate::currentDate();
    QMap<QString, int> livePause;
    for (const WorkSession &s : m_store.sessions(today)) {
        if (s.login.date() == today) {
            livePause[s.name] += s.pause / 60;
        }
    }
    const QString search = m_search->text().trimmed();
    QVector<Line> list;
    for (Attendance a : m_repository.attendance()) {
        if (!inPeriod(a.day()) || (!search.isEmpty() && !a.name().contains(search, Qt::CaseInsensitive))) {
            continue;
        }
        const bool live = a.day() == today && !a.departure().isValid();
        if (live) {
            Employee e;
            e.setId(a.employeeId());
            a.setPause(std::max(a.pause(), livePause.value(e.reference())));
        }
        Line line;
        line.employeeId = a.employeeId();
        line.name = archived(a.name(), a.employeeId());
        line.day = a.day();
        line.arrival = a.arrival();
        line.departure = a.departure();
        line.pause = a.pause();
        line.minutes = a.minutes();
        line.overtime = a.overtime();
        line.state = a.departure().isValid() ? QStringLiteral("Terminé")
            : live                           ? QStringLiteral("En cours")
                                             : QStringLiteral("À corriger");
        list.append(line);
    }
    std::stable_sort(list.begin(), list.end(), [](const Line &a, const Line &b) {
        if (a.day != b.day) {
            return a.day > b.day;
        }
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
    return list;
}

QVector<HoursView::Line> HoursView::totals(const QVector<Line> &list) const
{
    QMap<QString, Line> people;
    for (const Line &line : list) {
        const QString key = QString::number(line.employeeId) + line.name;
        if (!people.contains(key)) {
            people.insert(key, line);
            continue;
        }
        Line &sum = people[key];
        ++sum.days;
        sum.pause += line.pause;
        sum.minutes += line.minutes;
        sum.overtime += line.overtime;
    }
    QVector<Line> out;
    for (const Line &line : std::as_const(people)) {
        out.append(line);
    }
    std::stable_sort(out.begin(), out.end(), [](const Line &a, const Line &b) {
        return a.minutes > b.minutes;
    });
    return out;
}

void HoursView::refresh()
{
    const QVector<Line> list = lines(), people = totals(list);
    int minutes = 0, overtime = 0, pause = 0;
    for (const Line &line : list) {
        minutes += line.minutes;
        overtime += line.overtime;
        pause += line.pause;
    }
    m_values[0]->setText(QString::number(people.size()));
    m_values[1]->setText(duration(minutes));
    m_values[2]->setText(duration(overtime));
    m_values[3]->setText(duration(pause));

    QVector<QStringList> rows;
    if (m_total) {
        for (const Line &l : people) {
            rows.append({l.name, QString::number(l.days), duration(l.pause), duration(l.minutes),
                duration(l.minutes / l.days), duration(l.overtime)});
        }
    } else {
        for (const Line &l : list) {
            rows.append({l.day.toString("dd/MM/yyyy"), l.name, hm(l.arrival), hm(l.departure),
                QString::number(l.pause) + " min", duration(l.minutes), duration(l.overtime), l.state});
        }
    }
    if (rows == m_shown && !m_first) {
        return;
    }
    m_first = false;
    m_shown = rows;
    m_daily->setVisible(!m_total);
    m_summary->setVisible(m_total);
    QTableWidget *table = m_total ? m_summary : m_daily;
    const int overtimeColumn = m_total ? 5 : 6;
    table->setUpdatesEnabled(false);
    table->setRowCount(0);
    for (const QStringList &row : std::as_const(rows)) {
        addRow(table, row);
        if (row[overtimeColumn] != duration(0)) {
            table->item(table->rowCount() - 1, overtimeColumn)->setForeground(QColor("#e6c27a"));
        }
    }
    table->setUpdatesEnabled(true);
}

QString HoursView::html() const
{
    QStringList columns;
    const QTableWidget *table = m_total ? m_summary : m_daily;
    for (int c = 0; c < table->columnCount(); ++c) {
        columns << table->horizontalHeaderItem(c)->text();
    }
    QString body = QStringLiteral("<p>Période : %1 · employés : %2 · travail net : %3 · heures supplémentaires : %4</p>")
                       .arg(m_period->currentText().toHtmlEscaped(), m_values[0]->text(), m_values[1]->text(),
                           m_values[2]->text());
    body += "<table width='100%' cellspacing='0' cellpadding='5' border='1'>" + headers(columns);
    for (const QStringList &row : m_shown) {
        body += htmlRow(row);
    }
    return document(QStringLiteral("Pointage et heures"), body + "</table>");
}

void HoursView::exportPdf()
{
    refresh();
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Enregistrer le PDF"), "AUREA_heures.pdf", "Documents PDF (*.pdf)");
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(".pdf", Qt::CaseInsensitive)) {
        path += ".pdf";
    }
    QString error;
    if (!Reports::writePdf(path, html(), &error, true)) {
        DialogSkin::warn(this, "AUREA", error);
    } else {
        DialogSkin::info(this, "AUREA", QStringLiteral("PDF enregistré : ") + path);
    }
}
