#include <QtTest>
#include "inventoryrepository.h"
#include "materialwindow.h"
#include "qrservice.h"
#include "reports.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <limits>

class ModuleTests:public QObject {
    Q_OBJECT
private:
    static Material material(const QString &lot=QStringLiteral("SOI-TEST")) {
        Material m; m.name=QStringLiteral("Soie ivoire"); m.type=QStringLiteral("Tissu"); m.color=QStringLiteral("Ivoire"); m.supplier=QStringLiteral("Fournisseur couture");
        m.lot=lot; m.quantity=20; m.minimum=10; m.price=85; return m;
    }
private slots:
    void initTestCase() {
#ifdef Q_OS_WIN
        for(const QString &font:QStringList{"segoeui.ttf","segoeuib.ttf","georgia.ttf"})
            QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR","C:/Windows")).filePath("Fonts/"+font));
#endif
    }
    void crudAndPersistence() {
        QTemporaryDir dir; QVERIFY(dir.isValid()); const QString path=dir.filePath("stock.sqlite"); qint64 id=0;
        {
            InventoryRepository repo; QVERIFY2(repo.open(path),qPrintable(repo.error()));
            Material m=material(); QVERIFY(repo.save(m)); QVERIFY(m.id>0); id=m.id;
            QCOMPARE(repo.all().size(),1);
            m.name=QStringLiteral("Soie duchesse"); m.price=90; QVERIFY(repo.save(m));
            QCOMPARE(repo.find(id).name,m.name);
        }
        {
            InventoryRepository repo; QVERIFY(repo.open(path)); QCOMPARE(repo.find(id).price,90.0);
            QVERIFY(repo.move(id,-15)); QCOMPARE(repo.find(id).quantity,5.0); QVERIFY(repo.find(id).low());
            QVERIFY(repo.move(id,12)); QCOMPARE(repo.find(id).quantity,17.0);
            QVERIFY(repo.remove(id)); QCOMPARE(repo.all().size(),0);
            QVERIFY(!repo.findLot("SOI-TEST").id); QVERIFY(!repo.remove(id));
        }
    }
    void duplicateLotsAndSqlCharacters() {
        InventoryRepository repo; QVERIFY(repo.open(":memory:")); Material a=material(); QVERIFY(repo.save(a));
        Material b=material("soi-test"); QVERIFY(!repo.save(b)); QVERIFY(repo.error().contains("existe")); QCOMPARE(b.id,0);
        QCOMPARE(repo.all().size(),1);
        b=material("LOT-'_02"); b.name="Soie <b> & l'atelier"; b.supplier="Supplier'); DROP TABLE materials; --"; QVERIFY(repo.save(b));
        QCOMPARE(repo.findLot(b.lot).supplier,b.supplier); QCOMPARE(repo.all().size(),2);
        const QString html=StockReports::inventoryHtml(repo.all(),"Inventaire"); QVERIFY(html.contains("&lt;b&gt;")); QVERIFY(!html.contains("Soie <b>"));
    }
    void invalidValuesAndAtomicMovement() {
        InventoryRepository repo; QVERIFY(repo.open(":memory:")); Material m=material(); QVERIFY(repo.save(m));
        QVERIFY(!repo.move(m.id,-21)); QCOMPARE(repo.find(m.id).quantity,20.0);
        QVERIFY(!repo.move(m.id,0));
        QVERIFY(!repo.move(m.id,std::numeric_limits<double>::quiet_NaN()));
        Material invalid=material("SECOND"); invalid.quantity=-1; QVERIFY(!repo.save(invalid));
        invalid.quantity=5; invalid.name=""; QVERIFY(!repo.save(invalid));
        invalid=material("LOT ESPACE"); QVERIFY(!repo.save(invalid));
        m.quantity=10; QVERIFY(repo.save(m)); QCOMPARE(repo.find(m.id).quantity,10.0);
        m.unit="kg"; QVERIFY(!repo.save(m)); QCOMPARE(repo.find(m.id).unit,QStringLiteral("m"));
    }
    void discreteUnitsAndThreshold() {
        InventoryRepository repo; QVERIFY(repo.open(":memory:")); Material m=material(); m.unit=QStringLiteral("pièce"); m.quantity=10.5;
        QVERIFY(!repo.save(m)); m.quantity=10; QVERIFY(repo.save(m)); QVERIFY(!m.low());
        QVERIFY(!repo.move(m.id,0.5)); QCOMPARE(repo.find(m.id).quantity,10.0);
        QVERIFY(repo.move(m.id,-1)); QVERIFY(repo.find(m.id).low());
        m=repo.find(m.id); m.quantity=0; m.minimum=0; QVERIFY(repo.save(m)); QVERIFY(!m.low());
    }
    void qrRoundTripAndImages() {
        for(const QString &lot:QStringList{"SOI-2601",QStringLiteral("ÉTIQUETTE-026")}) {
            const QString code=QrService::link(material(lot)); const QImage qr=QrService::generate(code); QVERIFY(!qr.isNull());
            QString error; const QStringList decoded=QrService::decode(qr,&error); QVERIFY2(!decoded.isEmpty(),qPrintable(error));
            QCOMPARE(QrService::lotFromCode(decoded.first()),lot);
            QTransform transform; transform.rotate(90); const QStringList rotated=QrService::decode(qr.transformed(transform));
            QVERIFY(!rotated.isEmpty()); QCOMPARE(QrService::lotFromCode(rotated.first()),lot);
        }
        QVERIFY(QrService::lotFromCode("https://example.com").isEmpty()); QVERIFY(QrService::lotFromCode("AUREA:LOT:").isEmpty());
        QCOMPARE(QrService::lotFromCode("SOI-2601"),QStringLiteral("SOI-2601"));
        QCOMPARE(QrService::lotFromCode("AUREA:LOT:SOI-2601"),QStringLiteral("SOI-2601"));
        QImage blank(300,300,QImage::Format_RGB32); blank.fill(Qt::white); QString error; QVERIFY(QrService::decode(blank,&error).isEmpty()); QVERIFY(!error.isEmpty());
    }
    void pdfOutputs() {
        QTemporaryDir dir; InventoryRepository repo; QVERIFY(repo.open(":memory:",true)); const auto rows=repo.all(); QVERIFY(!rows.isEmpty());
        const QStringList documents={StockReports::inventoryHtml(rows,"Catalogue complet"),StockReports::purchaseHtml(rows.first(),12),StockReports::statisticsHtml(rows)};
        for(int i=0;i<documents.size();++i) {
            const QString path=dir.filePath(QString::number(i)+".pdf"); QString error;
            QVERIFY2(StockReports::writePdf(path,documents[i],&error,i==0),qPrintable(error)); QFile f(path); QVERIFY(f.open(QIODevice::ReadOnly)); QCOMPARE(f.read(5),QByteArray("%PDF-")); QVERIFY(f.size()>1000);
        }
        QString error; QVERIFY(StockReports::labelPdf(dir.filePath("label.pdf"),rows.first(),QrService::generate(QrService::link(rows.first())),&error));
        QVERIFY(!StockReports::writePdf(dir.filePath("absent/report.pdf"),documents.first(),&error));
        const QImage chart=StockReports::chart({},QSize(800,250)); QVERIFY(!chart.isNull());
    }
    void qrLinkOpensWebPage() {
        Material m=material(QStringLiteral("ÉTIQUETTE/026")); m.name=QStringLiteral("Soie <fine> & écrue");
        const QString code=QrService::link(m);
        QVERIFY(code.startsWith(QStringLiteral("https://itty.bitty.site/#Lot-%C3%89TIQUETTE%2F026/")));
        // La page cachée dans le lien : on la décompresse comme le site (on remet les 4 octets de taille pour qUncompress).
        QByteArray zlib=QByteArray::fromBase64(code.section('/',4).toLatin1()); zlib.prepend(QByteArray(4,'\0')); zlib[3]=char(0x7f);
        const QString page=QString::fromUtf8(qUncompress(zlib));
        QVERIFY(page.contains(QStringLiteral("<h1>Soie &lt;fine&gt; &amp; écrue</h1>"))); QVERIFY(page.contains(QStringLiteral("Stock : 20 m")));
        const auto qr=QrService::generate(code); const QStringList read=QrService::decode(qr);
        QVERIFY(!read.isEmpty()); QCOMPARE(QrService::lotFromCode(read.first()),m.lot);
        QCOMPARE(QrService::lotFromCode(code),m.lot);
        InventoryRepository repo; QVERIFY(repo.open(":memory:")); QVERIFY(repo.save(m));
        QVERIFY(repo.move(m.id,-7));
        QCOMPARE(repo.findLot(QrService::lotFromCode(code)).quantity,13.0);   // l'application lit le stock actuel
    }
    void uiWorkflow() {
        InventoryRepository repo; QVERIFY(repo.open(":memory:",true)); MaterialWindow window(repo); window.show(); QTest::qWait(20);
        auto *table=window.findChild<QTableWidget*>("materialsTable"); QVERIFY(table); QCOMPARE(table->rowCount(),8);
        auto *search=window.findChild<QLineEdit*>("searchEdit"); search->setText("Calais"); QCOMPARE(table->rowCount(),1); search->clear();
        auto *filter=window.findChild<QComboBox*>("stockFilter"); filter->setCurrentIndex(1); QCOMPARE(table->rowCount(),3); filter->setCurrentIndex(3); QCOMPARE(table->rowCount(),1); filter->setCurrentIndex(0);
        auto *sort=window.findChild<QComboBox*>("sortCombo"); sort->setCurrentIndex(1); QCOMPARE(table->item(0,0)->text(),QStringLiteral("Fermeture invisible"));
        auto *pages=window.findChild<QStackedWidget*>("pages");
        QTest::mouseClick(window.findChild<QPushButton*>("newMaterialButton"),Qt::LeftButton); QCOMPARE(pages->currentIndex(),1);
        window.findChild<QLineEdit*>("nameEdit")->setText("Organza de soie"); window.findChild<QLineEdit*>("lotEdit")->setText("ORG-TEST");
        window.findChild<QLineEdit*>("colorEdit")->setText("Rose poudré"); window.findChild<QLineEdit*>("supplierEdit")->setText("Maison couture");
        window.findChild<QDoubleSpinBox*>("quantityEdit")->setValue(25); window.findChild<QDoubleSpinBox*>("minimumEdit")->setValue(10); window.findChild<QDoubleSpinBox*>("priceEdit")->setValue(30);
        QTest::mouseClick(window.findChild<QPushButton*>("saveButton"),Qt::LeftButton); QCOMPARE(pages->currentIndex(),0); QCOMPARE(table->rowCount(),9);
        Material added=repo.findLot("ORG-TEST"); QVERIFY(added.id);
        QTest::mouseClick(window.findChild<QPushButton*>("editButton"),Qt::LeftButton); QCOMPARE(pages->currentIndex(),1);
        window.findChild<QDoubleSpinBox*>("quantityEdit")->setValue(8); QTest::mouseClick(window.findChild<QPushButton*>("saveButton"),Qt::LeftButton);
        QCOMPARE(repo.find(added.id).quantity,8.0); QCOMPARE(window.findChild<QLabel*>("alertsValue")->text(),QStringLiteral("4"));
        window.setPage(3); auto *code=window.findChild<QLineEdit*>("scanEdit"); code->setText(QStringLiteral("AUREA:LOT:ORG-TEST"));
        QTest::mouseClick(window.findChild<QPushButton*>("findLotButton"),Qt::LeftButton);
        QCOMPARE(window.findChild<QComboBox*>("lotCombo")->currentData().toLongLong(),added.id); QCOMPARE(repo.find(added.id).quantity,8.0);
        const QImage shown=window.findChild<QLabel*>("qrImage")->pixmap().toImage();
        const auto displayedCodes=QrService::decode(shown); QVERIFY(!displayedCodes.isEmpty());
        QCOMPARE(QrService::lotFromCode(displayedCodes.first()),added.lot);
        window.setPage(4); QTest::qWait(5); QVERIFY(window.findChild<QLabel*>("chartLabel")->pixmap().isNull()==false);
    }
    void emptyUi() {
        InventoryRepository repo; QVERIFY(repo.open(":memory:")); MaterialWindow window(repo);
        QCOMPARE(window.findChild<QTableWidget*>("materialsTable")->rowCount(),0);
        QVERIFY(!window.findChild<QLabel*>("logoMark")->pixmap().isNull());   // logo AUREA en haut à gauche
        if(qEnvironmentVariableIsSet("AUREA_SCREENSHOT")) { window.resize(1200,760); window.show(); QTest::qWait(50); window.grab().save(qEnvironmentVariable("AUREA_SCREENSHOT")); }
        QVERIFY(!window.findChild<QPushButton*>("consumeButton")->isEnabled()); QVERIFY(!window.findChild<QPushButton*>("purchaseOrderButton")->isEnabled());
        window.setPage(3); window.setPage(4);
    }
};
QTEST_MAIN(ModuleTests)
#include "test_materials.moc"
