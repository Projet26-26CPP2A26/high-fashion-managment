cmake_minimum_required(VERSION 3.16)
project(AureaEmployes VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS Crud)
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql PrintSupport SerialPort Network OpenGL)

add_executable(AureaEmployes WIN32 main.cpp
    Crud/crud_interface.cpp
    Crud/employee.cpp
    Crud/employeemodule.cpp
    Crud/employeerepository.cpp
    Crud/employeewindow.cpp
    Crud/oracleconnection.cpp
    Metier/affectation.cpp
    Metier/affectation_interface.cpp
    Metier/pdfexport.cpp
    Metier/pointage.cpp
    Metier/statistiques_pdf.cpp
    Suivi/Responsable/activityboard.cpp
    Suivi/Donnees/activitystore.cpp
    Suivi/Donnees/companylink.cpp
    Suivi/Employe/employeedesk.cpp
    Suivi/Responsable/employeepopup.cpp
    Suivi/Responsable/employeetile.cpp
    Suivi/Responsable/passagefeed.cpp
    Suivi/Responsable/reviewboard.cpp
    Suivi/Employe/submitdialog.cpp
    Suivi/Employe/timecard.cpp
    Arduino/badgelink.cpp
    Arduino/badgepage.cpp
    Arduino/badgeservice.cpp
    Arduino/virtualboard.cpp
    Animations/BadgeAnimation.cpp
    Animations/ClosingAnimation.cpp
    Animations/LinesAnimation.cpp
    Animations/LoginAnimation.cpp
    Animations/LogoAnimation.cpp
    Animations/TransitionAnimation.cpp
    Extras/Outils/companysimulator.cpp
    Extras/Interface/dialogskin.cpp
    Extras/Interface/errorbanner.cpp
    Extras/Interface/logindialog.cpp
    Extras/Outils/oraclecheck.cpp
    Extras/Outils/oraclemodulecheck.cpp
    Extras/Interface/polish.cpp
    Extras/Interface/uihelpers.cpp
    Extras/Interface/icons.cpp
    Extras/Interface/greeting.cpp
    Extras/Interface/sound.cpp
    Suivi/Donnees/taskcatalog.cpp
    Animations/UiAnimations.cpp
    Animations/ConnectionAnimation.cpp
    Crud/employee.h
    Crud/employeemodule.h
    Crud/employeerepository.h
    Crud/employeewindow.h
    Crud/oracleconnection.h
    Metier/pdfexport.h
    Suivi/Responsable/activityboard.h
    Suivi/Donnees/activitystore.h
    Suivi/Donnees/companylink.h
    Suivi/Employe/employeedesk.h
    Suivi/Responsable/employeepopup.h
    Suivi/Responsable/employeetile.h
    Suivi/Responsable/passagefeed.h
    Suivi/Responsable/reviewboard.h
    Suivi/Employe/submitdialog.h
    Suivi/Donnees/taskcatalog.h
    Suivi/Employe/timecard.h
    Arduino/badgelink.h
    Arduino/badgepage.h
    Arduino/badgeservice.h
    Arduino/virtualboard.h
    Animations/AnimationSpeed.h
    Animations/BadgeAnimation.h
    Animations/ClosingAnimation.h
    Animations/ConnectionAnimation.h
    Animations/LinesAnimation.h
    Animations/LoginAnimation.h
    Animations/LogoAnimation.h
    Animations/TransitionAnimation.h
    Animations/UiAnimations.h
    Extras/Outils/companysimulator.h
    Extras/Interface/dialogskin.h
    Extras/Interface/errorbanner.h
    Extras/Interface/greeting.h
    Extras/Interface/icons.h
    Extras/Interface/logindialog.h
    Extras/Outils/oraclecheck.h
    Extras/Outils/oraclemodulecheck.h
    Extras/Interface/polish.h
    Extras/Interface/sound.h
    Extras/Interface/uihelpers.h
    Crud/employees.ui
    Crud/employeewindow.ui
    Resources/assets.qrc)
target_include_directories(AureaEmployes PRIVATE Crud Metier Arduino Animations Suivi/Donnees Suivi/Employe Suivi/Responsable Extras/Interface Extras/Outils)
target_link_libraries(AureaEmployes PRIVATE Qt6::Widgets Qt6::Sql Qt6::PrintSupport Qt6::SerialPort Qt6::Network Qt6::OpenGL)
if(WIN32)
    target_link_libraries(AureaEmployes PRIVATE user32 dwmapi ole32 uuid)
endif()
