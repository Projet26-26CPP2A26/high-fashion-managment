QT += widgets sql charts
CONFIG += c++17
TEMPLATE = app
TARGET = AureaClients

INCLUDEPATH += common clients

SOURCES += main.cpp clients/clientwindow.cpp clients/clientdatabase.cpp \
    clients/client.cpp
HEADERS += common/icons.h common/appsetup.h clients/clientwindow.h clients/clientdatabase.h \
    clients/client.h
FORMS += clients/clientwindow.ui
RESOURCES += assets.qrc clients/clients.qrc
