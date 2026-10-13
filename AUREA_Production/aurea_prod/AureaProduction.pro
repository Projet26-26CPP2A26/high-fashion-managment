QT += widgets sql printsupport
CONFIG += c++17
TEMPLATE = app
TARGET = AureaProduction
SOURCES += main.cpp mainwindow.cpp pages.cpp uikit.cpp store.cpp database.cpp pdfexport.cpp
HEADERS += mainwindow.h pages.h uikit.h store.h database.h pdfexport.h
RESOURCES += assets.qrc
