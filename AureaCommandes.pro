QT += widgets sql printsupport charts
CONFIG += c++17
TEMPLATE = app
TARGET = AureaCommandes

INCLUDEPATH += common orders clients employees

SOURCES += main.cpp orders/orderwindow.cpp clients/clientdatabase.cpp employees/pdfexport.cpp
HEADERS += common/icons.h common/appsetup.h orders/orderwindow.h clients/clientdatabase.h employees/pdfexport.h
RESOURCES += assets.qrc chevrons.qrc orders/orders.qrc
