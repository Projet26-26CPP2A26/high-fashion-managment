# Alternative qmake (Qt Creator) — la configuration recommandée reste CMakeLists.txt
QT += widgets sql printsupport
CONFIG += c++17
TEMPLATE = app
TARGET = AureaMatieres

INCLUDEPATH += common materials materials/third_party/qrcodegen materials/third_party/quirc

SOURCES += main.cpp \
    common/oracleconnection.cpp common/oraclecheck.cpp \
    materials/materialwindow.cpp materials/inventoryrepository.cpp materials/qrservice.cpp materials/reports.cpp \
    materials/third_party/qrcodegen/qrcodegen.cpp materials/third_party/quirc/quirc.c \
    materials/third_party/quirc/decode.c materials/third_party/quirc/identify.c materials/third_party/quirc/version_db.c

HEADERS += common/icons.h common/oracleconnection.h common/oraclecheck.h common/sqldialect.h \
    materials/materialwindow.h materials/inventoryrepository.h materials/qrservice.h materials/reports.h \
    materials/third_party/qrcodegen/qrcodegen.hpp materials/third_party/quirc/quirc.h materials/third_party/quirc/quirc_internal.h

FORMS += materials/materialwindow.ui
RESOURCES += materials/materials.qrc
