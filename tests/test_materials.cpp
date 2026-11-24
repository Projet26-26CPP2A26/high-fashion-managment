#include <QtTest>
#include "inventoryrepository.h"
#include "qrservice.h"
#include "reports.h"
#include <QFile>
#include <QTemporaryDir>

class MaterialTests : public QObject {
    Q_OBJECT

private:
    static Material sampleMaterial()
    {
        Material m;
        m.setName("Soie ivoire");
        m.setType("Tissu");
        m.setColor("Ivoire");
        m.setSupplier("Fournisseur couture");
        m.setLot("SOI-TEST");
        m.setQuantity(20);
        m.setMinimum(10);
        m.setPrice(85);
        return m;
    }

private slots:
    void materialValidation()
    {
        Material m = sampleMaterial();
        QVERIFY(InventoryRepository::validate(m).isEmpty());
        m.setLot("SOI TEST");
        QVERIFY(!InventoryRepository::validate(m).isEmpty());
        m = sampleMaterial();
        m.setQuantity(-1);
        QVERIFY(!InventoryRepository::validate(m).isEmpty());
        m = sampleMaterial();
        m.setUnit("pièce");
        m.setQuantity(2.5);
        QVERIFY(!InventoryRepository::validate(m).isEmpty());
        m = sampleMaterial();
        m.setUnit("litre");
        QVERIFY(!InventoryRepository::validate(m).isEmpty());
    }

    void lowStock()
    {
        Material m = sampleMaterial();
        QVERIFY(!m.low());
        QCOMPARE(m.toOrder(), 0.0);
        m.setQuantity(5);
        QVERIFY(m.low());
        QCOMPARE(m.toOrder(), 15.0);
        const QString html = StockReports::allOrdersHtml({m, sampleMaterial()});
        QCOMPARE(html.count("SOI-TEST"), 1);
        QVERIFY(html.contains("15 m"));
    }

    void typeStatistics()
    {
        TypeStat stat;
        Material m = sampleMaterial();
        stat.add(m);
        m.setQuantity(5);
        stat.add(m);
        QCOMPARE(stat.count(), 2);
        QCOMPARE(stat.low(), 1);
        QCOMPARE(stat.value(), 25 * 85.0);
    }

    void qrRoundTrip()
    {
        const QString text = QrService::payload("SOI-TEST");
        QCOMPARE(QrService::lotFromCode(text), QString("SOI-TEST"));
        const QImage image = QrService::generate(text);
        QVERIFY(!image.isNull());
        QString error;
        const QStringList decoded = QrService::decode(image, &error);
        QVERIFY2(decoded.contains(text), qPrintable(error));

        const QString label = QrService::labelText(sampleMaterial(), QDateTime(QDate(2026, 11, 17), QTime(14, 30)));
        QVERIFY(label.startsWith("AUREA - Fiche matière\nLot : SOI-TEST\nMatière : Soie ivoire"));
        QVERIFY(label.contains("Date : 17/11/2026 14:30"));
        QCOMPARE(QrService::lotFromCode(label), QString("SOI-TEST"));
        const QString url = QrService::pageUrl(sampleMaterial(), QDateTime(QDate(2026, 11, 24), QTime(0, 0)));
        QCOMPARE(QrService::lotFromCode(url), QString("SOI-TEST"));
        QVERIFY(QrService::decode(QrService::generate(url), &error).contains(url));
        QVERIFY(QrService::lotFromCode("https://site.test/#SOI-TEST/abc").isEmpty());
        const QStringList labelDecoded = QrService::decode(QrService::generate(label), &error);
        QVERIFY2(labelDecoded.contains(label), qPrintable(error));
    }

    void pdfOutputs()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QVector<Material> materials = {sampleMaterial()};
        const QString path = dir.filePath("inventaire.pdf");
        QString error;
        QVERIFY2(StockReports::writePdf(path, StockReports::inventoryHtml(materials, "Test"), &error, true), qPrintable(error));
        QFile pdf(path);
        QVERIFY(pdf.open(QIODevice::ReadOnly));
        QCOMPARE(pdf.read(5), QByteArray("%PDF-"));
        QVERIFY(StockReports::statisticsHtml(materials).contains("Tissu"));
        QVERIFY(!StockReports::chart(materials, QSize(600, 300)).isNull());
    }
};

QTEST_MAIN(MaterialTests)
#include "test_materials.moc"
