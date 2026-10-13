QT += widgets sql printsupport charts
CONFIG += c++17
TEMPLATE = app
TARGET = AureaEmployes

INCLUDEPATH += common employees

SOURCES += main.cpp \
    employees/employeewindow.cpp employees/employeemodule.cpp employees/employeerepository.cpp \
    employees/database.cpp employees/pdfexport.cpp
HEADERS += common/icons.h common/appsetup.h \
    employees/employeewindow.h employees/employeemodule.h employees/employeerepository.h \
    employees/database.h employees/pdfexport.h
FORMS += employees/employeewindow.ui employees/employees.ui
RESOURCES += employees/assets.qrc
