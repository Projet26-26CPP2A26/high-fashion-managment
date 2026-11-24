QT += widgets sql printsupport serialport network opengl
CONFIG += c++17
QMAKE_CXXFLAGS_DEBUG += -O1
TEMPLATE = app
TARGET = AureaEmployes

INCLUDEPATH += Crud Metier Arduino Animations Suivi/Donnees Suivi/Employe Suivi/Responsable Extras/Interface Extras/Outils
win32: LIBS += -luser32 -ldwmapi -lole32 -luuid

SOURCES += \
    main.cpp \
    Crud/crud_interface.cpp \
    Crud/employee.cpp \
    Crud/employeemodule.cpp \
    Crud/employeerepository.cpp \
    Crud/employeewindow.cpp \
    Crud/oracleconnection.cpp \
    Metier/affectation.cpp \
    Metier/affectation_interface.cpp \
    Metier/heures.cpp \
    Metier/pdfexport.cpp \
    Metier/pointage.cpp \
    Metier/statistiques_pdf.cpp \
    Arduino/badgelink.cpp \
    Arduino/badgepage.cpp \
    Arduino/badgeservice.cpp \
    Arduino/virtualboard.cpp \
    Animations/ConnectionAnimation.cpp \
    Animations/UiAnimations.cpp \
    Animations/BadgeAnimation.cpp \
    Animations/ClosingAnimation.cpp \
    Animations/LinesAnimation.cpp \
    Animations/LoginAnimation.cpp \
    Animations/LogoAnimation.cpp \
    Animations/TransitionAnimation.cpp \
    Suivi/Donnees/taskcatalog.cpp \
    Suivi/Donnees/activitystore.cpp \
    Suivi/Donnees/companylink.cpp \
    Suivi/Employe/employeedesk.cpp \
    Suivi/Employe/submitdialog.cpp \
    Suivi/Employe/timecard.cpp \
    Suivi/Responsable/activityboard.cpp \
    Suivi/Responsable/employeepopup.cpp \
    Suivi/Responsable/employeetile.cpp \
    Suivi/Responsable/passagefeed.cpp \
    Suivi/Responsable/reviewboard.cpp \
    Extras/Interface/sound.cpp \
    Extras/Interface/greeting.cpp \
    Extras/Interface/icons.cpp \
    Extras/Interface/uihelpers.cpp \
    Extras/Interface/dialogskin.cpp \
    Extras/Interface/errorbanner.cpp \
    Extras/Interface/logindialog.cpp \
    Extras/Interface/polish.cpp \
    Extras/Outils/companysimulator.cpp \
    Extras/Outils/oraclecheck.cpp \
    Extras/Outils/oraclemodulecheck.cpp

HEADERS += \
    Crud/employee.h \
    Crud/employeemodule.h \
    Crud/employeerepository.h \
    Crud/employeewindow.h \
    Crud/oracleconnection.h \
    Metier/heures.h \
    Metier/pdfexport.h \
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
    Suivi/Donnees/activitystore.h \
    Suivi/Donnees/companylink.h \
    Suivi/Donnees/taskcatalog.h \
    Suivi/Employe/employeedesk.h \
    Suivi/Employe/submitdialog.h \
    Suivi/Employe/timecard.h \
    Suivi/Responsable/activityboard.h \
    Suivi/Responsable/employeepopup.h \
    Suivi/Responsable/employeetile.h \
    Suivi/Responsable/passagefeed.h \
    Suivi/Responsable/reviewboard.h \
    Extras/Interface/dialogskin.h \
    Extras/Interface/errorbanner.h \
    Extras/Interface/greeting.h \
    Extras/Interface/icons.h \
    Extras/Interface/logindialog.h \
    Extras/Interface/polish.h \
    Extras/Interface/sound.h \
    Extras/Interface/uihelpers.h \
    Extras/Outils/companysimulator.h \
    Extras/Outils/oraclecheck.h \
    Extras/Outils/oraclemodulecheck.h

FORMS += \
    Crud/employees.ui \
    Crud/employeewindow.ui

RESOURCES += Resources/assets.qrc

DISTFILES += \
    Arduino/pointage_badge/pointage_badge.ino \
    oracle/01_schema.sql \
    oracle/02_demo.sql \
    oracle/03_badges.sql \
    oracle/99_drop_all.sql
