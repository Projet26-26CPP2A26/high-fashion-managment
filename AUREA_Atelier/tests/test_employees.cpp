#include <QtTest>
#include "database.h"
#include "employeerepository.h"
#include "employeemodule.h"
#include "employeewindow.h"
#include "pdfexport.h"
#include <QComboBox>
#include <QFile>
#include <QFontDatabase>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>

class EmployeeTests:public QObject {
    Q_OBJECT
    static Employee employee(QString email="sarah@aurea.tn") {
        Employee e; e.name="Sarah Ben Ali"; e.email=email; e.position="Couturier"; e.specialty="Couture haute précision"; e.hired=QDate(2020,1,1); e.baseLoad=35; return e;
    }
private slots:
    void initTestCase() {
        for(const QString &file:QStringList{"segoeui.ttf","segoeuib.ttf","georgia.ttf"}) QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR","C:/Windows")).filePath("Fonts/"+file));
    }
    void employeePersistence() {
        QTemporaryDir folder; const QString path=folder.filePath("employees.sqlite"); qint64 id=0;
        {
            Database database; QVERIFY(database.open(path)); EmployeeRepository people(database.database()); QVERIFY(people.initialize());
            Employee e=employee(); QVERIFY(people.save(e)); id=e.id;
            QVERIFY(people.assign(id,"Assemblage",e.specialty,8)); QCOMPARE(people.find(id).load,55);
        }
        {
            Database database; QVERIFY(database.open(path)); EmployeeRepository people(database.database()); QVERIFY(people.initialize());
            QCOMPARE(people.find(id).load,55); QCOMPARE(people.assignments().size(),1);
        }
    }
    void validationAndDuplicateEmail() {
        Database stock; QVERIFY(stock.open(":memory:")); EmployeeRepository people(stock.database()); QVERIFY(people.initialize());
        Employee e=employee(); QVERIFY(people.save(e)); Employee duplicate=employee("SARAH@AUREA.TN"); QVERIFY(!people.save(duplicate)); QCOMPARE(people.all().size(),1);
        Employee future=employee("future@aurea.tn"); future.hired=QDate::currentDate().addDays(1); QVERIFY(!people.save(future));
        future.hired=QDate(2020,1,1); future.baseLoad=101; QVERIFY(!people.save(future)); future.baseLoad=0; future.email="invalid"; QVERIFY(!people.save(future));
        e.name="Sarah <b> & l'atelier"; QVERIFY(people.save(e)); QVERIFY(people.remove(e.id)); QVERIFY(!people.remove(e.id));
    }
    void assignmentCapacityAndCompletion() {
        Database stock; QVERIFY(stock.open(":memory:")); EmployeeRepository people(stock.database()); QVERIFY(people.initialize());
        Employee e=employee(); QVERIFY(people.save(e)); QVERIFY(people.assign(e.id,"Robe","Couture haute précision",24)); QCOMPARE(people.find(e.id).load,95);
        QVERIFY(!people.assign(e.id,"Surcharge","Couture haute précision",3)); QCOMPARE(people.assignments().size(),1); QCOMPARE(people.find(e.id).load,95);
        auto ranking=people.rank("Couture haute précision",3); QCOMPARE(ranking.size(),1); QVERIFY(!ranking[0].fits);
        Employee changed=people.find(e.id); changed.baseLoad=50; QVERIFY(!people.save(changed)); QCOMPARE(people.find(e.id).baseLoad,35);
        const qint64 task=people.assignments()[0].id; QVERIFY(people.completeAssignment(task)); QCOMPARE(people.find(e.id).load,35); QVERIFY(!people.completeAssignment(task));
        e=people.find(e.id); e.available=false; QVERIFY(people.save(e)); QVERIFY(people.rank(e.specialty,8).isEmpty()); QVERIFY(!people.assign(e.id,"Indisponible",e.specialty,8));
    }
    void attendanceValidationAndHistory() {
        Database stock; QVERIFY(stock.open(":memory:")); EmployeeRepository people(stock.database()); QVERIFY(people.initialize());
        QVERIFY(!people.clockIn(0)); QVERIFY(people.attendance().isEmpty()); Employee e=employee(); QVERIFY(people.save(e)); const QDate day=QDate::currentDate();
        QVERIFY(!people.saveAttendance(e.id,day,QTime(8,0),QTime(7,0),0)); QVERIFY(!people.saveAttendance(e.id,day,QTime(8,0),QTime(9,0),60));
        QVERIFY(!people.saveAttendance(e.id,day.addDays(1),QTime(8,0),QTime(17,0),60));
        QVERIFY(people.saveAttendance(e.id,day,QTime(8,0),QTime(18,0),60)); QCOMPARE(people.attendance()[0].minutes(),540); QCOMPARE(people.attendance()[0].overtime(),60);
        QVERIFY(people.saveAttendance(e.id,day,QTime(9,0),QTime(17,0),30)); QCOMPARE(people.attendance().size(),1); QCOMPARE(people.attendance()[0].minutes(),450);
        QVERIFY(people.assign(e.id,"Finitions",e.specialty,8)); QVERIFY(people.remove(e.id)); QCOMPARE(people.attendance().size(),1); QCOMPARE(people.attendance()[0].employeeId,0); QCOMPARE(people.assignments()[0].name,e.name);
        const qint64 old=e.id; Employee replacement=employee(); QVERIFY(people.save(replacement)); QVERIFY(replacement.id>old); QCOMPARE(people.find(replacement.id).load,35);
    }
    void liveAttendanceUsesSameNetCalculation() {
        Database stock; QVERIFY(stock.open(":memory:")); EmployeeRepository people(stock.database()); QVERIFY(people.initialize()); Employee e=employee(); QVERIFY(people.save(e));
        QVERIFY(people.clockIn(e.id,QDate::currentDate(),QTime(8,0))); QVERIFY(!people.clockIn(e.id,QDate::currentDate(),QTime(8,5))); QVERIFY(!people.clockOut(e.id,QDate::currentDate(),QTime(7,0)));
        auto a=people.attendance()[0]; QCOMPARE(a.pause,0); QCOMPARE(a.minutes(QTime(17,0)),540); QCOMPARE(a.overtime(QTime(17,0)),60);
        QVERIFY(people.clockOut(e.id,QDate::currentDate(),QTime(17,0))); QCOMPARE(people.attendance()[0].minutes(),540); QVERIFY(!people.clockOut(e.id,QDate::currentDate(),QTime(18,0)));
    }
    void demoSeedsOnlyOnceAndStockStartsEmpty() {
        Database stock; QVERIFY(stock.open(":memory:")); EmployeeRepository people(stock.database()); QVERIFY(people.initialize()); QVERIFY(people.all().isEmpty());
        QVERIFY(people.initialize(true)); QCOMPARE(people.all().size(),11); for(const auto &e:people.all()) QVERIFY(people.remove(e.id));
        QVERIFY(people.initialize(true)); QVERIFY(people.all().isEmpty());
    }
    void standaloneWindowAndFilteredReports() {
        Database database; QVERIFY(database.open(":memory:")); EmployeeWindow window(database.database(),true); window.show(); QCoreApplication::processEvents();
        QVERIFY(window.ready()); auto *module=window.module(); QVERIFY(module);
        const QStringList names={"navDashboard","navEmployees","navAssignment","navAttendance"};
        for(int i=0;i<4;++i) {auto *button=window.findChild<QPushButton*>(names[i]); QVERIFY(button); QTest::mouseClick(button,Qt::LeftButton); QCOMPARE(module->findChild<QTabWidget*>("employeeTabs")->currentIndex(),i);}
        QTest::mouseClick(window.findChild<QPushButton*>("navEmployees"),Qt::LeftButton);
        QTest::mouseClick(module->findChild<QPushButton*>("openStatsButton"),Qt::LeftButton); QCOMPARE(module->findChild<QTabWidget*>("employeeTabs")->currentIndex(),4);
        QTest::mouseClick(module->findChild<QPushButton*>("statsBackButton"),Qt::LeftButton); QCOMPARE(module->findChild<QTabWidget*>("employeeTabs")->currentIndex(),1);
        QVERIFY(!module->findChild<QPushButton*>("assignButton")->isEnabled());
        module->findChild<QComboBox*>("taskTemplate")->setCurrentIndex(2); QCOMPARE(module->findChild<QComboBox*>("taskSpecialty")->currentText(),QStringLiteral("Broderie main"));
        QVERIFY(module->findChild<QPushButton*>("assignButton")->isEnabled());
        auto *search=module->findChild<QLineEdit*>("employeeSearch"); search->setText("Nour"); QCOMPARE(module->findChild<QTableWidget*>("employeesTable")->rowCount(),1);
        QVERIFY(module->employeesHtml().contains("Nour Hammami")); QVERIFY(!module->employeesHtml().contains("Sarah Ben Ali"));
        search->clear(); module->findChild<QComboBox*>("employeeSort")->setCurrentIndex(6);
        QCOMPARE(module->findChild<QTableWidget*>("employeesTable")->item(0,7)->text(),QStringLiteral("0 %"));
        QTemporaryDir folder; QString error; const QString path=folder.filePath("employees.pdf"); QVERIFY(Reports::writePdf(path,module->employeesHtml(),&error,true)); QFile pdf(path); QVERIFY(pdf.open(QIODevice::ReadOnly)); QVERIFY(pdf.read(5)=="%PDF-");
        QCOMPARE(module->repository().all().size(),11);
    }

};
QTEST_MAIN(EmployeeTests)
#include "test_employees.moc"
