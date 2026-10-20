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
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

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
            Material m=material(); QVERIFY(repo.save(m,{})); QVERIFY(m.id>0); id=m.id;
            QCOMPARE(repo.all().size(),1); QCOMPARE(repo.history().size(),1);
            m.name=QStringLiteral("Soie duchesse"); m.price=90; QVERIFY(repo.save(m,"Mise à jour"));
            QCOMPARE(repo.find(id).name,m.name); QCOMPARE(repo.history().size(),1);
        }
        {
            InventoryRepository repo; QVERIFY(repo.open(path)); QCOMPARE(repo.find(id).price,90.0);
            QVERIFY(repo.move(id,-15,"Commande robe 001")); QCOMPARE(repo.find(id).quantity,5.0); QVERIFY(repo.find(id).low());
            QVERIFY(repo.move(id,12,"Réception fournisseur")); QCOMPARE(repo.find(id).quantity,17.0);
            QVERIFY(repo.remove(id)); QCOMPARE(repo.all().size(),0); QCOMPARE(repo.history().size(),3);
            QVERIFY(!repo.findLot("SOI-TEST").id); QVERIFY(!repo.remove(id));
        }
    }
    void duplicateLotsAndSqlCharacters() {
        InventoryRepository repo; QVERIFY(repo.open(":memory:")); Material a=material(); QVERIFY(repo.save(a,{}));
        Material b=material("soi-test"); QVERIFY(!repo.save(b,{})); QVERIFY(repo.error().contains("existe")); QCOMPARE(b.id,0);
        QCOMPARE(repo.all().size(),1); QCOMPARE(repo.history().size(),1);
        b=material("LOT-'_02"); b.name="Soie <b> & l'atelier"; b.supplier="Supplier'); DROP TABLE materials; --"; QVERIFY(repo.save(b,{}));
        QCOMPARE(repo.findLot(b.lot).supplier,b.supplier); QCOMPARE(repo.all().size(),2);
        const QString html=StockReports::inventoryHtml(repo.all(),"Inventaire"); QVERIFY(html.contains("&lt;b&gt;")); QVERIFY(!html.contains("Soie <b>"));
    }
    void invalidValuesAndAtomicMovement() {
        InventoryRepository repo; QVERIFY(repo.open(":memory:")); Material m=material(); QVERIFY(repo.save(m,{}));
        QVERIFY(!repo.move(m.id,-21,"Consommation")); QCOMPARE(repo.find(m.id).quantity,20.0); QCOMPARE(repo.history().size(),1);
        QVERIFY(!repo.move(m.id,1," ")); QVERIFY(!repo.move(m.id,0,"Invalide"));
        QVERIFY(!repo.move(m.id,std::numeric_limits<double>::quiet_NaN(),"Invalide"));
        Material invalid=material("SECOND"); invalid.quantity=-1; QVERIFY(!repo.save(invalid,{}));
        invalid.quantity=5; invalid.name=""; QVERIFY(!repo.save(invalid,{}));
        invalid=material("LOT ESPACE"); QVERIFY(!repo.save(invalid,{}));
        m.quantity=10; QVERIFY(!repo.save(m,"")); QCOMPARE(repo.find(m.id).quantity,20.0);
        QVERIFY(repo.save(m,"Correction comptage")); QCOMPARE(repo.history().first().kind,QStringLiteral("Correction"));
        m.unit="kg"; QVERIFY(!repo.save(m,"Unité")); QCOMPARE(repo.find(m.id).unit,QStringLiteral("m"));
    }
    void discreteUnitsAndThreshold() {
        InventoryRepository repo; QVERIFY(repo.open(":memory:")); Material m=material(); m.unit=QStringLiteral("pièce"); m.quantity=10.5;
        QVERIFY(!repo.save(m,{})); m.quantity=10; QVERIFY(repo.save(m,{})); QVERIFY(!m.low());
        QVERIFY(!repo.move(m.id,0.5,"Fraction")); QCOMPARE(repo.find(m.id).quantity,10.0);
        QVERIFY(repo.move(m.id,-1,"Bouton utilisé")); QVERIFY(repo.find(m.id).low());
        m=repo.find(m.id); m.quantity=0; m.minimum=0; QVERIFY(repo.save(m,"Correction")); QVERIFY(!m.low());
    }
    void qrRoundTripAndImages() {
        for(const QString &lot:QStringList{"SOI-2601",QStringLiteral("ÉTIQUETTE-026")}) {
            const QString code=QrService::payload(lot); const QImage qr=QrService::generate(code); QVERIFY(!qr.isNull());
            QString error; const QStringList decoded=QrService::decode(qr,&error); QVERIFY2(decoded.contains(code),qPrintable(error));
            QCOMPARE(QrService::lotFromCode(decoded.first()),lot);
            QTransform transform; transform.rotate(90); QVERIFY(QrService::decode(qr.transformed(transform)).contains(code));
            QTemporaryDir dir; QVERIFY(qr.save(dir.filePath("qr.png"))); QVERIFY(QrService::decode(QImage(dir.filePath("qr.png"))).contains(code));
        }
        QVERIFY(QrService::lotFromCode("https://example.com").isEmpty()); QVERIFY(QrService::lotFromCode("AUREA:LOT:").isEmpty());
        QCOMPARE(QrService::lotFromCode("SOI-2601"),QStringLiteral("SOI-2601"));
        QImage blank(300,300,QImage::Format_RGB32); blank.fill(Qt::white); QString error; QVERIFY(QrService::decode(blank,&error).isEmpty()); QVERIFY(!error.isEmpty());
    }
    void pdfOutputs() {
        QTemporaryDir dir; InventoryRepository repo; QVERIFY(repo.open(":memory:",true)); const auto rows=repo.all(); QVERIFY(!rows.isEmpty());
        const QStringList documents={StockReports::inventoryHtml(rows,"Catalogue complet"),StockReports::purchaseHtml(rows.first(),12),StockReports::statisticsHtml(rows)};
        for(int i=0;i<documents.size();++i) {
            const QString path=dir.filePath(QString::number(i)+".pdf"); QString error;
            QVERIFY2(StockReports::writePdf(path,documents[i],&error,i==0),qPrintable(error)); QFile f(path); QVERIFY(f.open(QIODevice::ReadOnly)); QCOMPARE(f.read(5),QByteArray("%PDF-")); QVERIFY(f.size()>1000);
        }
        QString error; QVERIFY(StockReports::labelPdf(dir.filePath("label.pdf"),rows.first(),QrService::generate(QrService::payload(rows.first().lot)),&error));
        QVERIFY(!StockReports::writePdf(dir.filePath("absent/report.pdf"),documents.first(),&error));
        const QImage chart=StockReports::chart({},QSize(800,250)); QVERIFY(!chart.isNull());
    }
    void phoneQrCarriesStandaloneSnapshot() {
        Material m=material(QStringLiteral("ÉTIQUETTE-026")); m.name=QStringLiteral("Soie <script> & écrue");
        const auto date=QDateTime::fromString("2026-10-12T12:00:00Z",Qt::ISODate);
        const QString code=QrService::phonePayload(m,date); const QUrl url(code);
        QCOMPARE(url.scheme(),QStringLiteral("https")); QVERIFY(!url.host().contains("127.0.0.1")); QVERIFY(url.query().isEmpty());
        const auto bytes=QByteArray::fromBase64(url.fragment().mid(9).toLatin1(),QByteArray::Base64UrlEncoding);
        const auto snapshot=QJsonDocument::fromJson(qUncompress(bytes)).object();
        QCOMPARE(snapshot.value("n").toString(),m.name); QCOMPARE(snapshot.value("s").toString(),m.supplier);
        QCOMPARE(snapshot.value("q").toDouble(),20.0); QCOMPARE(snapshot.value("d").toString(),QStringLiteral("2026-10-12T12:00:00Z"));
        const auto qr=QrService::generate(code); QVERIFY(!qr.isNull()); QVERIFY(QrService::decode(qr).contains(code));
        QCOMPARE(QrService::lotFromCode(code),m.lot);
        // The browser decoder uses this fixture to verify the real Qt wire format.
        if(qEnvironmentVariableIsSet("AUREA_QR_FIXTURE")) {
            QFile fixture(qEnvironmentVariable("AUREA_QR_FIXTURE")); QVERIFY(fixture.open(QIODevice::WriteOnly)); fixture.write(code.toUtf8());
        }
        InventoryRepository repo; QVERIFY(repo.open(":memory:")); QVERIFY(repo.save(m,{}));
        QVERIFY(repo.move(m.id,-7,"Consommation après impression"));
        QCOMPARE(repo.findLot(QrService::lotFromCode(code)).quantity,13.0);
        QCOMPARE(snapshot.value("q").toDouble(),20.0); // Printed snapshot stays dated; desktop reads current stock.
        QVERIFY(QrService::phonePayload(repo.find(m.id),date)!=code);
    }
    void phoneQrRejectsInvalidSnapshots() {
        QVERIFY(QrService::lotFromCode(QrService::readerUrl()+"#aurea=z1.invalid").isEmpty());
        QByteArray oversized(6,'x'); oversized[0]=0x7f;
        QVERIFY(QrService::lotFromCode(QrService::readerUrl()+"#aurea=z1."+oversized.toBase64(QByteArray::Base64UrlEncoding)).isEmpty());
        const auto wrongVersion=qCompress("{\"v\":2,\"l\":\"LOT-01\"}").toBase64(QByteArray::Base64UrlEncoding);
        QVERIFY(QrService::lotFromCode(QrService::readerUrl()+"#aurea=z1."+wrongVersion).isEmpty());
        QCOMPARE(QrService::lotFromCode("http://192.168.1.2:48762/aurea/lot/SOI-TEST?token=old"),QStringLiteral("SOI-TEST"));
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
        window.setPage(3); auto *code=window.findChild<QLineEdit*>("scanEdit"); code->setText(QrService::payload("ORG-TEST"));
        QTest::mouseClick(window.findChild<QPushButton*>("findLotButton"),Qt::LeftButton);
        QCOMPARE(window.findChild<QComboBox*>("lotCombo")->currentData().toLongLong(),added.id); QCOMPARE(repo.find(added.id).quantity,8.0);
        const QImage shown=window.findChild<QLabel*>("qrImage")->pixmap().toImage();
        const auto displayedCodes=QrService::decode(shown); QVERIFY(!displayedCodes.isEmpty());
        QCOMPARE(QrService::lotFromCode(displayedCodes.first()),added.lot);
        window.setPage(4); QTest::qWait(5); QVERIFY(window.findChild<QLabel*>("chartLabel")->pixmap().isNull()==false);
        window.setPage(5); QVERIFY(window.findChild<QTableWidget*>("historyTable")->rowCount()>=9);
    }
    void emptyUi() {
        InventoryRepository repo; QVERIFY(repo.open(":memory:")); MaterialWindow window(repo);
        QCOMPARE(window.findChild<QTableWidget*>("materialsTable")->rowCount(),0);
        QVERIFY(!window.findChild<QPushButton*>("consumeButton")->isEnabled()); QVERIFY(!window.findChild<QPushButton*>("purchaseOrderButton")->isEnabled());
        window.setPage(3); window.setPage(4); window.setPage(5);
    }
};
QTEST_MAIN(ModuleTests)
#include "test_materials.moc"
