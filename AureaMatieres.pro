QT += widgets sql printsupport serialport network
CONFIG += c++17
TEMPLATE = app
TARGET = AureaMatieres

INCLUDEPATH += Crud Metier arduino Animations Extras third_party/qrcodegen third_party/quirc
win32: LIBS += -lwinmm

SOURCES += main.cpp \
    Crud/crud_interface.cpp \
    Crud/inventoryrepository.cpp \
    Crud/materialwindow.cpp \
    Crud/oracleconnection.cpp \
    Metier/qr_interface.cpp \
    Metier/qrservice.cpp \
    Metier/reapprovisionnement.cpp \
    Metier/reports.cpp \
    Metier/statistiques_pdf.cpp \
    arduino/arduino.cpp \
    arduino/borne_interface.cpp \
    arduino/bornepage.cpp \
    Animations/QrAnimation.cpp \
    Animations/StitchAnimation.cpp \
    third_party/qrcodegen/qrcodegen.cpp \
    third_party/quirc/quirc.c third_party/quirc/decode.c \
    third_party/quirc/identify.c third_party/quirc/version_db.c

HEADERS += Crud/inventoryrepository.h \
    Crud/materialwindow.h \
    Crud/oracleconnection.h \
    Metier/qrservice.h \
    Metier/reports.h \
    arduino/arduino.h \
    arduino/bornepage.h \
    Animations/LinkLight.h \
    Animations/Motion.h \
    Animations/QrAnimation.h \
    Animations/StitchAnimation.h \
    Animations/UiEffects.h \
    Extras/icons.h \
    Extras/sound.h \
    Extras/uihelpers.h \
    third_party/qrcodegen/qrcodegen.hpp third_party/quirc/quirc.h third_party/quirc/quirc_internal.h

FORMS += Crud/materialwindow.ui
RESOURCES += Resources/materials.qrc
