QT += widgets sql printsupport serialport network opengl
CONFIG += c++17
TEMPLATE = app
TARGET = AureaEmployes

INCLUDEPATH += connexion crud metiers suivi arduino extras
win32: LIBS += -luser32 -ldwmapi -lwinmm

SOURCES += main.cpp \
    connexion/logindialog.cpp \
    connexion/oracleconnection.cpp \
    crud/crud_interface.cpp \
    crud/employee.cpp \
    crud/employeemodule.cpp \
    crud/employeerepository.cpp \
    crud/employeewindow.cpp \
    metiers/affectation.cpp \
    metiers/affectation_interface.cpp \
    metiers/pdfexport.cpp \
    metiers/pointage.cpp \
    metiers/pointage_interface.cpp \
    metiers/statistiques_pdf.cpp \
    suivi/activityboard.cpp \
    suivi/activitystore.cpp \
    suivi/companylink.cpp \
    suivi/employeedesk.cpp \
    suivi/employeepopup.cpp \
    suivi/reviewboard.cpp \
    suivi/submitdialog.cpp \
    arduino/badgepage.cpp \
    arduino/badgeservice.cpp \
    arduino/virtualboard.cpp \
    extras/closingview.cpp \
    extras/companysimulator.cpp \
    extras/dialogskin.cpp \
    extras/employeetile.cpp \
    extras/living.cpp \
    extras/loginscene.cpp \
    extras/morphoverlay.cpp \
    extras/oraclecheck.cpp \
    extras/oraclemodulecheck.cpp \
    extras/polish.cpp \
    extras/spectacle.cpp

HEADERS += connexion/logindialog.h \
    connexion/oracleconnection.h \
    crud/employee.h \
    crud/employeemodule.h \
    crud/employeerepository.h \
    crud/employeewindow.h \
    metiers/pdfexport.h \
    suivi/activityboard.h \
    suivi/activitystore.h \
    suivi/companylink.h \
    suivi/employeedesk.h \
    suivi/employeepopup.h \
    suivi/reviewboard.h \
    suivi/submitdialog.h \
    suivi/taskcatalog.h \
    arduino/badgepage.h \
    arduino/badgeservice.h \
    arduino/linklight.h \
    arduino/virtualboard.h \
    extras/closingview.h \
    extras/companysimulator.h \
    extras/dialogskin.h \
    extras/employeetile.h \
    extras/fx.h \
    extras/greeting.h \
    extras/icons.h \
    extras/living.h \
    extras/loginscene.h \
    extras/morphoverlay.h \
    extras/motion.h \
    extras/oraclecheck.h \
    extras/oraclemodulecheck.h \
    extras/polish.h \
    extras/sound.h \
    extras/spectacle.h \
    extras/uihelpers.h

FORMS += crud/employees.ui \
    crud/employeewindow.ui

RESOURCES += extras/assets.qrc
