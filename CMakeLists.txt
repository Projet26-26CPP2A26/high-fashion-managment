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
    Metier/pointage_interface.cpp
    Metier/statistiques_pdf.cpp
    Suivi/activityboard.cpp
    Suivi/activitystore.cpp
    Suivi/companylink.cpp
    Suivi/employeedesk.cpp
    Suivi/employeepopup.cpp
    Suivi/employeetile.cpp
    Suivi/passagefeed.cpp
    Suivi/reviewboard.cpp
    Suivi/submitdialog.cpp
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
    Extras/companysimulator.cpp
    Extras/dialogskin.cpp
    Extras/logindialog.cpp
    Extras/oraclecheck.cpp
    Extras/oraclemodulecheck.cpp
    Extras/polish.cpp
    Crud/employee.h
    Crud/employeemodule.h
    Crud/employeerepository.h
    Crud/employeewindow.h
    Crud/oracleconnection.h
    Metier/pdfexport.h
    Suivi/activityboard.h
    Suivi/activitystore.h
    Suivi/companylink.h
    Suivi/employeedesk.h
    Suivi/employeepopup.h
    Suivi/employeetile.h
    Suivi/passagefeed.h
    Suivi/reviewboard.h
    Suivi/submitdialog.h
    Suivi/taskcatalog.h
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
    Extras/companysimulator.h
    Extras/dialogskin.h
    Extras/greeting.h
    Extras/icons.h
    Extras/logindialog.h
    Extras/oraclecheck.h
    Extras/oraclemodulecheck.h
    Extras/polish.h
    Extras/sound.h
    Extras/uihelpers.h
    Crud/employees.ui
    Crud/employeewindow.ui
    Resources/assets.qrc)
target_include_directories(AureaEmployes PRIVATE Crud Metier Suivi Arduino Animations Extras)
target_link_libraries(AureaEmployes PRIVATE Qt6::Widgets Qt6::Sql Qt6::PrintSupport Qt6::SerialPort Qt6::Network Qt6::OpenGL)
if(WIN32)
    target_link_libraries(AureaEmployes PRIVATE user32 dwmapi winmm)
endif()
