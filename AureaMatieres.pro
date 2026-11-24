QT += widgets sql printsupport serialport network opengl
CONFIG += c++17
TEMPLATE = app
TARGET = AureaMatieres

INCLUDEPATH += Crud Metier arduino Animations Extras Extras/libs/qrcodegen Extras/libs/quirc
win32: LIBS += -lwinmm
RC_ICONS = Resources/Assets/aurea.ico

HEADERS += \
    Crud/material.h \
    Crud/materialrepository.h \
    Crud/materialwindow.h \
    Crud/oracleconnection.h \
    Metier/pdfexport.h \
    Metier/qrcode.h \
    arduino/arduino.h \
    arduino/bornepage.h \
    Animations/AnimationSpeed.h \
    Animations/CardAnimation.h \
    Animations/ChartAnimation.h \
    Animations/ClosingAnimation.h \
    Animations/ConnectionAnimation.h \
    Animations/FormAnimation.h \
    Animations/LogoAnimation.h \
    Animations/OrderAnimation.h \
    Animations/QrAnimation.h \
    Animations/TableAnimation.h \
    Animations/TransitionAnimation.h \
    Animations/UiAnimations.h \
    Extras/dialogskin.h \
    Extras/icons.h \
    Extras/sound.h \
    Extras/uihelpers.h \
    Extras/libs/qrcodegen/qrcodegen.hpp \
    Extras/libs/quirc/quirc.h \
    Extras/libs/quirc/quirc_internal.h

SOURCES += \
    main.cpp \
    Crud/crud_interface.cpp \
    Crud/material.cpp \
    Crud/materialrepository.cpp \
    Crud/materialwindow.cpp \
    Crud/oracleconnection.cpp \
    Metier/pdfexport.cpp \
    Metier/qrcode.cpp \
    Metier/qrcode_interface.cpp \
    Metier/reapprovisionnement_interface.cpp \
    Metier/statistiques_pdf.cpp \
    arduino/arduino.cpp \
    arduino/borne_interface.cpp \
    arduino/bornepage.cpp \
    Animations/CardAnimation.cpp \
    Animations/ChartAnimation.cpp \
    Animations/ClosingAnimation.cpp \
    Animations/FormAnimation.cpp \
    Animations/LogoAnimation.cpp \
    Animations/OrderAnimation.cpp \
    Animations/QrAnimation.cpp \
    Animations/TableAnimation.cpp \
    Animations/TransitionAnimation.cpp \
    Extras/dialogskin.cpp \
    Extras/libs/qrcodegen/qrcodegen.cpp \
    Extras/libs/quirc/decode.c \
    Extras/libs/quirc/identify.c \
    Extras/libs/quirc/quirc.c \
    Extras/libs/quirc/version_db.c

FORMS += Crud/materialwindow.ui

RESOURCES += Resources/assets.qrc
