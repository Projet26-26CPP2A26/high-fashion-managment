# Alternative qmake (Qt Creator) — la configuration recommandée reste CMakeLists.txt
QT += widgets sql printsupport charts
CONFIG += c++17
TEMPLATE = app
TARGET = AureaAtelier

INCLUDEPATH += common employees materials production shell \
    materials/third_party/qrcodegen materials/third_party/quirc

SOURCES += main.cpp \
    common/oracleconnection.cpp common/oraclecheck.cpp common/oraclemodulecheck.cpp \
    shell/shellwindow.cpp \
    production/productionwindow.cpp production/productionpages.cpp production/productionstore.cpp production/productionkit.cpp \
    materials/materialwindow.cpp materials/inventoryrepository.cpp materials/qrservice.cpp materials/reports.cpp \
    materials/third_party/qrcodegen/qrcodegen.cpp materials/third_party/quirc/quirc.c \
    materials/third_party/quirc/decode.c materials/third_party/quirc/identify.c materials/third_party/quirc/version_db.c \
    employees/employeewindow.cpp employees/employeemodule.cpp employees/employeerepository.cpp \
    employees/database.cpp employees/pdfexport.cpp

HEADERS += common/icons.h common/oracleconnection.h common/oraclecheck.h common/oraclemodulecheck.h common/sqldialect.h \
    shell/shellwindow.h \
    production/productionwindow.h production/productionpages.h production/productionstore.h production/productionkit.h \
    materials/materialwindow.h materials/inventoryrepository.h materials/qrservice.h materials/reports.h \
    materials/third_party/qrcodegen/qrcodegen.hpp materials/third_party/quirc/quirc.h materials/third_party/quirc/quirc_internal.h \
    employees/employeewindow.h employees/employeemodule.h employees/employeerepository.h \
    employees/database.h employees/pdfexport.h

FORMS += employees/employeewindow.ui employees/employees.ui materials/materialwindow.ui
RESOURCES += employees/assets.qrc materials/materials.qrc production/production.qrc shell/shell.qrc
