cmake_minimum_required(VERSION 3.16)
project(AureaEmployes VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql PrintSupport SerialPort Network OpenGL)

add_executable(AureaEmployes WIN32 main.cpp
    connexion/logindialog.cpp
    connexion/oracleconnection.cpp
    crud/crud_interface.cpp
    crud/employee.cpp
    crud/employeemodule.cpp
    crud/employeerepository.cpp
    crud/employeewindow.cpp
    metiers/affectation.cpp
    metiers/affectation_interface.cpp
    metiers/pdfexport.cpp
    metiers/pointage.cpp
    metiers/pointage_interface.cpp
    metiers/statistiques_pdf.cpp
    suivi/activityboard.cpp
    suivi/activitystore.cpp
    suivi/companylink.cpp
    suivi/employeedesk.cpp
    suivi/employeepopup.cpp
    suivi/reviewboard.cpp
    suivi/submitdialog.cpp
    arduino/badgepage.cpp
    arduino/badgeservice.cpp
    arduino/virtualboard.cpp
    extras/closingview.cpp
    extras/companysimulator.cpp
    extras/dialogskin.cpp
    extras/employeetile.cpp
    extras/living.cpp
    extras/loginscene.cpp
    extras/morphoverlay.cpp
    extras/oraclecheck.cpp
    extras/oraclemodulecheck.cpp
    extras/polish.cpp
    extras/spectacle.cpp
    connexion/logindialog.h
    connexion/oracleconnection.h
    crud/employee.h
    crud/employeemodule.h
    crud/employeerepository.h
    crud/employeewindow.h
    metiers/pdfexport.h
    suivi/activityboard.h
    suivi/activitystore.h
    suivi/companylink.h
    suivi/employeedesk.h
    suivi/employeepopup.h
    suivi/reviewboard.h
    suivi/submitdialog.h
    suivi/taskcatalog.h
    arduino/badgepage.h
    arduino/badgeservice.h
    arduino/linklight.h
    arduino/virtualboard.h
    extras/closingview.h
    extras/companysimulator.h
    extras/dialogskin.h
    extras/employeetile.h
    extras/fx.h
    extras/greeting.h
    extras/icons.h
    extras/living.h
    extras/loginscene.h
    extras/morphoverlay.h
    extras/motion.h
    extras/oraclecheck.h
    extras/oraclemodulecheck.h
    extras/polish.h
    extras/sound.h
    extras/spectacle.h
    extras/uihelpers.h
    crud/employees.ui
    crud/employeewindow.ui
    extras/assets.qrc)
target_include_directories(AureaEmployes PRIVATE connexion crud metiers suivi arduino extras)
target_link_libraries(AureaEmployes PRIVATE Qt6::Widgets Qt6::Sql Qt6::PrintSupport Qt6::SerialPort Qt6::Network Qt6::OpenGL)
if(WIN32)
    target_link_libraries(AureaEmployes PRIVATE user32 dwmapi winmm)
endif()
