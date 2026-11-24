#include "employeepopup.h"
#include "AnimationSpeed.h"
#include "activityboard.h"
#include "employeetile.h"
#include "UiAnimations.h"
#include "polish.h"
#include "taskcatalog.h"
#include <QComboBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

class WeekChart : public QWidget {
public:
    explicit WeekChart(QWidget *parent = nullptr) : QWidget(parent) { setMinimumHeight(240); }
    void setDays(const QVector<int> &seconds) { if (m_days != seconds) { m_days = seconds; update(); } }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (m_days.isEmpty()) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double top = std::max(WorkRules::Day * 1.25, double(*std::max_element(m_days.begin(), m_days.end())));
        const QRectF plot(10, 26, width() - 20, height() - 62);
        const double limit = plot.bottom() - plot.height() * WorkRules::Day / top;
        p.setPen(QPen(QColor("#5a5548"), 1, Qt::DashLine));
        p.drawLine(QPointF(plot.left(), limit), QPointF(plot.right(), limit));
        p.setPen(QColor("#b3aea3"));
        p.drawText(QPointF(plot.left(), limit - 5), QStringLiteral("8 h"));
        const int n = int(m_days.size());
        const double slot = plot.width() / n, bar = std::min(46.0, slot * 0.5);
        for (int i = 0; i < n; ++i) {
            const double x = plot.left() + slot * i + (slot - bar) / 2, h = plot.height() * m_days[i] / top;
            const double normal = plot.height() * std::min(m_days[i], WorkRules::Day) / top;
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#d9c48f"));
            p.drawRoundedRect(QRectF(x, plot.bottom() - normal, bar, normal), 5, 5);
            if (h > normal) {
                p.setBrush(QColor("#e08f8f"));
                p.drawRoundedRect(QRectF(x, plot.bottom() - h, bar, h - normal), 5, 5);
            }
            p.setPen(QColor("#b9b4a8"));
            const QRectF label(plot.left() + slot * i, plot.bottom() + 6, slot, 20);
            p.drawText(label, Qt::AlignCenter, QLocale().toString(QDate::currentDate().addDays(i - n + 1), "ddd d"));
            if (m_days[i] > 0) p.drawText(QRectF(label.left(), plot.bottom() - h - 20, slot, 18), Qt::AlignCenter, clockText(m_days[i]).left(5));
        }
    }

private:
    QVector<int> m_days;
};

namespace {
QLabel *text(const QString &value, const char *role)
{
    auto *l = new QLabel(value);
    l->setTextFormat(Qt::PlainText);
    l->setProperty(role, true);
    return l;
}
}

EmployeePopup::EmployeePopup(ActivityBoard &board, ActivityStore &store, QWidget *window) : QWidget(window), m_board(board), m_store(store)
{
    setFocusPolicy(Qt::StrongFocus);
    hide();
    window->installEventFilter(this);
    m_panel = new QFrame(this);
    m_panel->setObjectName("popup");
    m_panel->setStyleSheet("QFrame#popup{background:#1f1f22;border:1px solid #4a4434;border-radius:18px;}");
    auto *root = new QVBoxLayout(m_panel);
    root->setContentsMargins(26, 22, 26, 22);
    root->setSpacing(14);

    auto *head = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    m_name = text(QString(), "kpi");
    m_job = text(QString(), "muted");
    titles->setSpacing(2);
    titles->addWidget(m_name);
    titles->addWidget(m_job);
    m_status = new QLabel;
    auto *previous = new QPushButton(QStringLiteral("‹")), *next = new QPushButton(QStringLiteral("›")), *close = new QPushButton(QStringLiteral("Fermer"));
    head->addLayout(titles, 1);
    head->addWidget(m_status);
    head->addSpacing(10);
    for (QPushButton *b : {previous, next}) { b->setFixedWidth(40); head->addWidget(b); }
    head->addWidget(close);
    root->addLayout(head);

    auto *tabs = new QHBoxLayout;
    const QStringList names = {QStringLiteral("Aperçu"), QStringLiteral("Tâches"), QStringLiteral("Activité"), QStringLiteral("Semaine")};
    for (int i = 0; i < names.size(); ++i) {
        auto *b = new QPushButton(names[i]);
        b->setProperty("nav", true);
        b->setCheckable(true);
        b->setAutoExclusive(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet("text-align:center");
        connect(b, &QPushButton::clicked, this, [this, i] { showTab(i); });
        tabs->addWidget(b);
        m_tabs.append(b);
    }
    root->addLayout(tabs);
    m_pages = new QStackedWidget;
    root->addWidget(m_pages, 1);

    auto *overview = new QWidget;
    auto *grid = new QGridLayout(overview);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(12);
    const QStringList captions = {QStringLiteral("Tâche en cours"), QStringLiteral("Avancement"), QStringLiteral("Tâche suivante"), QStringLiteral("Tâches validées"),
                                  QStringLiteral("Temps travaillé"), QStringLiteral("Heures supplémentaires"), QStringLiteral("Pause"), QStringLiteral("Inactivité · PC en veille")};
    for (int i = 0; i < 8; ++i) {
        auto *frame = new QFrame;
        frame->setProperty("card", true);
        auto *box = new QVBoxLayout(frame);
        box->setContentsMargins(16, 12, 16, 12);
        box->addWidget(text(captions[i], "muted"));
        m_values[i] = text(QString(), "section");
        m_values[i]->setWordWrap(true);
        box->addWidget(m_values[i]);
        box->addStretch();
        grid->addWidget(frame, i / 2, i % 2);
    }
    m_pages->addWidget(overview);

    auto *tasks = new QWidget;
    auto *tasksBox = new QVBoxLayout(tasks);
    tasksBox->setContentsMargins(0, 0, 0, 0);
    tasksBox->setSpacing(10);
    auto *form = new QHBoxLayout;
    m_title = new QComboBox;
    m_title->setEditable(true);
    m_title->lineEdit()->setMaxLength(120);
    m_title->lineEdit()->setPlaceholderText(QStringLiteral("Choisir ou saisir une tâche…"));
    m_minutes = new QSpinBox;
    m_minutes->setRange(5, 480);
    m_minutes->setSingleStep(5);
    m_minutes->setValue(45);
    m_minutes->setSuffix(" min");
    auto *send = new QPushButton(QStringLiteral("Affecter"));
    send->setProperty("primary", true);
    form->addWidget(m_title, 1);
    form->addWidget(m_minutes);
    form->addWidget(send);
    tasksBox->addLayout(form);
    m_notice = text(QString(), "muted");
    m_notice->setStyleSheet("color:#f0b9bb");
    m_notice->hide();
    tasksBox->addWidget(m_notice);
    auto *holder = new QWidget;
    holder->setObjectName("dashboardContents");
    holder->setStyleSheet("QWidget#dashboardContents{background:transparent}");
    m_tasks = new QVBoxLayout(holder);
    m_tasks->setContentsMargins(0, 0, 6, 0);
    m_tasks->setSpacing(8);
    m_tasks->setAlignment(Qt::AlignTop);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setStyleSheet("background:transparent");
    scroll->setWidget(holder);
    tasksBox->addWidget(scroll, 1);
    m_pages->addWidget(tasks);

    m_log = new QTableWidget(0, 2);
    m_log->setHorizontalHeaderLabels({QStringLiteral("Heure"), QStringLiteral("Événement")});
    m_log->horizontalHeader()->setStretchLastSection(true);
    m_log->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_log->verticalHeader()->hide();
    m_log->verticalHeader()->setDefaultSectionSize(36);
    m_log->setShowGrid(false);
    m_log->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_log->setSelectionMode(QAbstractItemView::NoSelection);
    m_pages->addWidget(m_log);

    auto *week = new QWidget;
    auto *weekBox = new QVBoxLayout(week);
    weekBox->setContentsMargins(0, 0, 0, 0);
    weekBox->addWidget(text(QStringLiteral("Temps travaillé sur 7 jours · en rouge : heures supplémentaires"), "muted"));
    m_week = new WeekChart;
    weekBox->addWidget(m_week, 1);
    m_pages->addWidget(week);

    connect(previous, &QPushButton::clicked, this, [this] { step(-1); });
    connect(next, &QPushButton::clicked, this, [this] { step(1); });
    connect(close, &QPushButton::clicked, this, &EmployeePopup::closePopup);
    connect(send, &QPushButton::clicked, this, &EmployeePopup::assign);
    connect(m_title, &QComboBox::activated, this, [this](int i) { if (m_title->itemData(i).isValid()) m_minutes->setValue(m_title->itemData(i).toInt()); });
    TablePolish::apply(this);
    m_animation.setDuration(Motion::Base);
    m_animation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) { m_open = v.toReal(); place(); update(); });
    connect(&m_animation, &QAbstractAnimation::finished, this, [this] { if (m_open < 0.01) hide(); });
}

void EmployeePopup::openFor(const QString &reference)
{
    const bool wasOpen = isVisible() && !m_reference.isEmpty();
    m_reference = reference;
    m_tasksShown.clear();
    m_notice->hide();
    const Employee e = m_board.employee(reference);
    m_specialty = e.specialty();
    m_title->clear();
    for (const TaskModel &t : TaskCatalog::forSpecialty(m_specialty)) m_title->addItem(t.title, t.minutes);
    m_title->setCurrentIndex(-1);
    m_title->clearEditText();
    if (!wasOpen) {
        m_tabs[0]->setChecked(true);
        m_pages->setCurrentIndex(0);
    }
    refresh();
    setGeometry(parentWidget()->rect());
    show();
    raise();
    setFocus();
    m_animation.stop();
    m_animation.setStartValue(wasOpen ? 1.0 : 0.0);
    m_animation.setEndValue(1.0);
    m_animation.start();
}

void EmployeePopup::closePopup()
{
    if (m_reference.isEmpty()) return;
    m_reference.clear();
    m_animation.stop();
    m_animation.setStartValue(m_open);
    m_animation.setEndValue(0.0);
    m_animation.start();
}

void EmployeePopup::place()
{
    const int w = std::min(760, width() - 60), h = std::min(600, height() - 60);
    m_panel->setGeometry((width() - w) / 2, (height() - h) / 2 + int(14 * (1 - m_open)), w, h);
}

void EmployeePopup::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, int(165 * m_open)));
}

bool EmployeePopup::eventFilter(QObject *object, QEvent *event)
{
    if (object == parentWidget() && event->type() == QEvent::Resize) {
        setGeometry(parentWidget()->rect());
        place();
    }
    return QWidget::eventFilter(object, event);
}

void EmployeePopup::mousePressEvent(QMouseEvent *event) { if (!m_panel->geometry().contains(event->pos())) closePopup(); }

void EmployeePopup::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) closePopup();
    else if (event->key() == Qt::Key_Left) step(-1);
    else if (event->key() == Qt::Key_Right) step(1);
    else QWidget::keyPressEvent(event);
}

void EmployeePopup::showTab(int index)
{
    m_tabs[index]->setChecked(true);
    Fx::switchPage(m_pages, index);
}

void EmployeePopup::step(int delta)
{
    const QStringList order = m_board.order();
    if (order.isEmpty()) return;
    const int n = int(order.size()), at = std::max(0, int(order.indexOf(m_reference)));
    openFor(order[(at + delta + n) % n]);
}

void EmployeePopup::assign()
{
    const QString title = m_title->currentText().trimmed();
    if (title.isEmpty() || m_reference.isEmpty()) return;
    const QString problem = m_board.assignTask(m_reference, title, m_minutes->value());
    m_notice->setText(problem);
    m_notice->setVisible(!problem.isEmpty());
    if (!problem.isEmpty()) return;
    m_title->setCurrentIndex(-1);
    m_title->clearEditText();
    refresh();
}

void EmployeePopup::refresh()
{
    if (m_reference.isEmpty()) return;
    const WorkSession s = m_board.session(m_reference);
    const Employee e = m_board.employee(m_reference);
    if (!e.id()) { closePopup(); return; }
    m_name->setText(e.name());
    m_job->setText(e.reference() + QStringLiteral(" · ") + e.position() + QStringLiteral(" · ") + e.specialty());
    const QColor color = statusColor(s.status());
    m_status->setText(QStringLiteral("●  ") + s.status());
    m_status->setStyleSheet("color:" + color.name() + ";font-weight:600");
    const ReviewStats stats = m_store.reviewStats(m_reference);
    const int left = WorkRules::Pause - s.pause;
    const QStringList values = {
        s.task.isEmpty() ? QStringLiteral("—") : s.task,
        s.taskPlanned > 0 ? QStringLiteral("%1 % · ").arg(int(s.taskSpent * 100LL / s.taskPlanned)) + clockText(s.taskSpent) + " / " + clockText(s.taskPlanned) : QStringLiteral("—"),
        s.nextTask.isEmpty() ? QStringLiteral("—") : s.nextTask,
        QString::number(stats.accepted) + (stats.rating > 0 ? QStringLiteral(" · qualité %1/5 ★").arg(stats.rating, 0, 'f', 1) : QString()),
        clockText(s.work) + (s.login.isValid() ? QStringLiteral(" · arrivée ") + s.login.toString("HH:mm") + (s.logout.isValid() ? QStringLiteral(", départ ") + s.logout.toString("HH:mm") : QString()) : QString()),
        clockText(s.work - WorkRules::Day),
        s.pause == 0 ? QStringLiteral("Pas encore prise") : QStringLiteral("%1 min · ").arg(s.pause / 60) + (left < 0 ? QStringLiteral("dépassée") : QStringLiteral("reste %1 min").arg(left / 60)),
        clockText(s.idle) + " · " + clockText(s.off)};
    for (int i = 0; i < 8; ++i) m_values[i]->setText(values[i]);
    showTasks();
    const QVector<ActivityEvent> events = m_store.events(QDate::currentDate(), m_reference);
    m_log->setRowCount(int(events.size()));
    for (int row = 0; row < events.size(); ++row) {
        const QString cells[] = {events[row].at.toString("HH:mm:ss"), events[row].text};
        for (int c = 0; c < 2; ++c) {
            if (!m_log->item(row, c)) m_log->setItem(row, c, new QTableWidgetItem);
            m_log->item(row, c)->setText(cells[c]);
        }
    }
    m_week->setDays(m_store.dailyWork(m_reference, 7));
}

void EmployeePopup::showTasks()
{
    static const QStringList states = {QStringLiteral("À faire"), QStringLiteral("En cours"), QStringLiteral("Validée"), QStringLiteral("En revue"), QStringLiteral("À corriger")};
    static const QStringList colors = {"#b9b4a8", "#e2cf9f", "#7fc08a", "#e6c27a", "#e3a06d"};
    const QVector<WorkTask> list = m_store.tasks(m_reference);
    QString signature = m_reference;
    for (const WorkTask &t : list) signature += QStringLiteral("|%1:%2:%3").arg(t.id).arg(t.state).arg(t.stepsDone);
    if (signature == m_tasksShown) return;
    m_tasksShown = signature;
    while (QLayoutItem *item = m_tasks->takeAt(0)) {
        if (item->widget()) { item->widget()->hide(); item->widget()->deleteLater(); }
        delete item;
    }
    if (list.isEmpty()) m_tasks->addWidget(text(QStringLiteral("Aucune tâche affectée."), "muted"));
    for (const WorkTask &t : list) {
        auto *frame = new QFrame;
        frame->setProperty("card", true);
        auto *row = new QHBoxLayout(frame);
        row->setContentsMargins(16, 10, 16, 10);
        int done = 0;
        for (int i = 0; i < t.steps.size(); ++i) done += (t.stepsDone >> i) & 1;
        auto *box = new QVBoxLayout;
        box->addWidget(text(t.title, "section"));
        box->addWidget(text(QStringLiteral("Prévu %1 min").arg(t.planned / 60) + (t.steps.isEmpty() ? QString() : QStringLiteral(" · étapes %1/%2").arg(done).arg(t.steps.size())), "muted"));
        auto *state = new QLabel(states.value(t.state) + (t.state == WorkTask::Done && t.rating > 0 ? QStringLiteral("  %1/5 ★").arg(t.rating) : QString()));
        state->setStyleSheet("font-weight:600;color:" + colors.value(t.state));
        row->addLayout(box, 1);
        row->addWidget(state);
        m_tasks->addWidget(frame);
    }
}
