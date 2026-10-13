#include "qrservice.h"
#include "inventoryrepository.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>
#include "qrcodegen.hpp"
extern "C" {
#include "quirc.h"
}
#include <cstring>
#include <memory>
#include <QUrl>

QString QrService::payload(const QString &lot) { return QStringLiteral("AUREA:LOT:")+lot; }
QString QrService::readerUrl() { return QStringLiteral("https://aurea-lot-reader.achref-zargua4.chatgpt.site/"); }
QString QrService::phonePayload(const Material &m, const QDateTime &recorded) {
    const QJsonObject snapshot{{"v",1},{"l",m.lot},{"n",m.name},{"t",m.type},{"c",m.color},
        {"s",m.supplier},{"u",m.unit},{"q",m.quantity},{"m",m.minimum},{"p",m.price},
        {"d",recorded.toUTC().toString(Qt::ISODate)}};
    const QByteArray data=QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
    return readerUrl()+QStringLiteral("#aurea=z1.")+QString::fromLatin1(qCompress(data,9).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals));
}
QString QrService::lotFromCode(const QString &code) {
    QString text=code.trimmed();
    if(text.startsWith(QStringLiteral("https://"),Qt::CaseInsensitive)) {
        const QUrl url(text,QUrl::StrictMode);
        const QString fragment=url.fragment();
        if(!url.isValid()||url.host().isEmpty()||!fragment.startsWith(QStringLiteral("aurea=z1."))) return {};
        const QByteArray encoded=fragment.mid(9).toLatin1();
        if(encoded.isEmpty()||encoded.size()>6000) return {};
        const auto decoded=QByteArray::fromBase64Encoding(encoded,QByteArray::Base64UrlEncoding|QByteArray::AbortOnBase64DecodingErrors);
        if(!decoded||decoded.decoded.size()<6) return {};
        const quint32 size=qFromBigEndian<quint32>(decoded.decoded.constData());
        if(size==0||size>8192) return {};
        const QByteArray data=qUncompress(decoded.decoded);
        if(data.size()!=size) return {};
        const QJsonDocument document=QJsonDocument::fromJson(data);
        if(!document.isObject()||document.object().value("v").toInt()!=1) return {};
        text=document.object().value("l").toString();
    }
    else if(text.startsWith(QStringLiteral("http://"),Qt::CaseInsensitive)) {
        const QUrl url(text,QUrl::StrictMode);
        if(!url.isValid()||!url.path().startsWith(QStringLiteral("/aurea/lot/"))||url.host().isEmpty()) return {};
        text=url.path().mid(11);
    }
    else if(text.startsWith(QStringLiteral("AUREA:LOT:"))) text=text.mid(10);
    else if(text.contains(':')) return {};
    if(text.isEmpty()||text.size()>80) return {};
    for(const QChar c:text) if(c.isSpace()||c==':'||!c.isPrint()) return {};
    return text;
}
QImage QrService::generate(const QString &text,int scale) {
    if(text.isEmpty()||scale<1||scale>32) return {};
    try {
        const QByteArray bytes=text.toUtf8();
        const auto qr=qrcodegen::QrCode::encodeText(bytes.constData(),qrcodegen::QrCode::Ecc::MEDIUM);
        const int border=4, extent=(qr.getSize()+border*2)*scale;
        QImage image(extent,extent,QImage::Format_RGB32); image.fill(Qt::white);
        for(int y=0;y<qr.getSize();++y) for(int x=0;x<qr.getSize();++x) if(qr.getModule(x,y))
            for(int dy=0;dy<scale;++dy) {
                QRgb *line=reinterpret_cast<QRgb*>(image.scanLine((y+border)*scale+dy));
                for(int dx=0;dx<scale;++dx) line[(x+border)*scale+dx]=qRgb(0,0,0);
            }
        return image;
    } catch(const std::exception &) { return {}; }
}
QStringList QrService::decode(const QImage &source,QString *error) {
    if(error) error->clear();
    if(source.isNull()) { if(error) *error=QStringLiteral("Cette image ne peut pas être ouverte."); return {}; }
    QImage image=source;
    if(image.width()>2400||image.height()>2400) image=image.scaled(2400,2400,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    image=image.convertToFormat(QImage::Format_Grayscale8);
    std::unique_ptr<quirc,decltype(&quirc_destroy)> scanner(quirc_new(),quirc_destroy);
    if(!scanner||quirc_resize(scanner.get(),image.width(),image.height())<0) {
        if(error) *error=QStringLiteral("Mémoire insuffisante pour lire ce QR code.");
        return {};
    }
    int w=0,h=0; uint8_t *buffer=quirc_begin(scanner.get(),&w,&h);
    for(int y=0;y<h;++y) std::memcpy(buffer+y*w,image.constScanLine(y),static_cast<size_t>(w));
    quirc_end(scanner.get()); QStringList results;
    for(int i=0;i<quirc_count(scanner.get());++i) {
        quirc_code code{}; quirc_data data{}; quirc_extract(scanner.get(),i,&code);
        auto status=quirc_decode(&code,&data);
        if(status==QUIRC_ERROR_DATA_ECC) { quirc_flip(&code); status=quirc_decode(&code,&data); }
        if(status==QUIRC_SUCCESS) results.append(QString::fromUtf8(reinterpret_cast<const char*>(data.payload),data.payload_len));
    }
    results.removeDuplicates();
    if(results.isEmpty() && error) *error=QStringLiteral("Aucun QR code lisible. Choisissez une image nette contenant le code complet.");
    return results;
}
