QT += widgets sql printsupport serialport network opengl
CONFIG += c++17
TEMPLATE = app
TARGET = AureaEmployes

INCLUDEPATH += Crud Metier Suivi Arduino Animations Extras
win32: LIBS += -luser32 -ldwmapi -lwinmm

SOURCES += main.cpp \
    Crud/crud_interface.cpp \
    Crud/employee.cpp \
    Crud/employeemodule.cpp \
    Crud/employeerepository.cpp \
    Crud/employeewindow.cpp \
    Crud/oracleconnection.cpp \
    Metier/affectation.cpp \
    Metier/affectation_interface.cpp \
    Metier/pdfexport.cpp \
    Metier/pointage.cpp \
    Metier/statistiques_pdf.cpp \
    Suivi/activityboard.cpp \
    Suivi/activitystore.cpp \
    Suivi/companylink.cpp \
    Suivi/employeedesk.cpp \
    Suivi/employeepopup.cpp \
    Suivi/employeetile.cpp \
    Suivi/passagefeed.cpp \
    Suivi/reviewboard.cpp \
    Suivi/submitdialog.cpp \
    Arduino/badgelink.cpp \
    Arduino/badgepage.cpp \
    Arduino/badgeservice.cpp \
    Arduino/virtualboard.cpp \
    Animations/BadgeAnimation.cpp \
    Animations/ClosingAnimation.cpp \
    Animations/LinesAnimation.cpp \
    Animations/LoginAnimation.cpp \
    Animations/LogoAnimation.cpp \
    Animations/TransitionAnimation.cpp \
    Extras/companysimulator.cpp \
    Extras/dialogskin.cpp \
    Extras/logindialog.cpp \
    Extras/oraclecheck.cpp \
    Extras/oraclemodulecheck.cpp \
    Extras/polish.cpp

HEADERS += Crud/employee.h \
    Crud/employeemodule.h \
    Crud/employeerepository.h \
    Crud/employeewindow.h \
    Crud/oracleconnection.h \
    Metier/pdfexport.h \
    Suivi/activityboard.h \
    Suivi/activitystore.h \
    Suivi/companylink.h \
    Suivi/employeedesk.h \
    Suivi/employeepopup.h \
    Suivi/employeetile.h \
    Suivi/passagefeed.h \
    Suivi/reviewboard.h \
    Suivi/submitdialog.h \
    Suivi/taskcatalog.h \
    Arduino/badgelink.h \
    Arduino/badgepage.h \
    Arduino/badgeservice.h \
    Arduino/virtualboard.h \
    Animations/AnimationSpeed.h \
    Animations/BadgeAnimation.h \
    Animations/ClosingAnimation.h \
    Animations/ConnectionAnimation.h \
    Animations/LinesAnimation.h \
    Animations/LoginAnimation.h \
    Animations/LogoAnimation.h \
    Animations/TransitionAnimation.h \
    Animations/UiAnimations.h \
    Extras/companysimulator.h \
    Extras/dialogskin.h \
    Extras/greeting.h \
    Extras/icons.h \
    Extras/logindialog.h \
    Extras/oraclecheck.h \
    Extras/oraclemodulecheck.h \
    Extras/polish.h \
    Extras/sound.h \
    Extras/uihelpers.h

FORMS += Crud/employees.ui \
    Crud/employeewindow.ui

RESOURCES += Resources/assets.qrc
