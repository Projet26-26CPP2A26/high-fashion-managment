# Alternative qmake (Qt Creator) — la configuration recommandée reste CMakeLists.txt
QT += widgets sql printsupport network svg
CONFIG += c++17
CONFIG += console
TEMPLATE = app
TARGET = AureaMatieres

INCLUDEPATH += common materials

SOURCES += main.cpp \
    common/oracleconnection.cpp common/oraclecheck.cpp \
    materials/materialwindow.cpp materials/inventoryrepository.cpp materials/qrservice.cpp materials/reports.cpp

HEADERS += common/icons.h common/oracleconnection.h common/oraclecheck.h common/sqldialect.h \
    materials/materialwindow.h materials/inventoryrepository.h materials/qrservice.h materials/reports.h

FORMS += materials/materialwindow.ui
RESOURCES += materials/materials.qrc
