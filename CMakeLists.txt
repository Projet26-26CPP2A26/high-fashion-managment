cmake_minimum_required(VERSION 3.16)
project(AureaMatieres VERSION 1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/Crud)
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql PrintSupport SerialPort Network)

add_library(quirc STATIC
    Extras/libs/quirc/quirc.c Extras/libs/quirc/decode.c
    Extras/libs/quirc/identify.c Extras/libs/quirc/version_db.c)
target_include_directories(quirc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Extras/libs/quirc)

set(APP_SOURCES
    Crud/crud_interface.cpp Crud/material.cpp Crud/materialrepository.cpp Crud/materialwindow.cpp Crud/materialwindow.h
    Crud/materialwindow.ui Crud/oracleconnection.cpp
    Metier/pdfexport.cpp Metier/qrcode.cpp Metier/qrcode_interface.cpp Metier/reapprovisionnement_interface.cpp Metier/statistiques_pdf.cpp
    arduino/arduino.cpp arduino/borne_interface.cpp arduino/bornepage.cpp
    Animations/ChartAnimation.cpp Animations/ChartAnimation.h
    Animations/ClosingAnimation.cpp Animations/ClosingAnimation.h
    Animations/LinesAnimation.cpp Animations/LinesAnimation.h
    Animations/LogoAnimation.cpp Animations/LogoAnimation.h
    Animations/QrAnimation.cpp Animations/QrAnimation.h
    Animations/TableAnimation.cpp Animations/TableAnimation.h
    Animations/TransitionAnimation.cpp Animations/TransitionAnimation.h
    Extras/dialogskin.cpp Extras/libs/qrcodegen/qrcodegen.cpp Resources/assets.qrc)

set(APP_INCLUDES Crud Metier arduino Animations Extras Extras/libs/qrcodegen)
set(APP_LIBRARIES quirc Qt6::Widgets Qt6::Sql Qt6::PrintSupport Qt6::SerialPort Qt6::Network winmm)

add_executable(AureaMatieres WIN32 main.cpp ${APP_SOURCES})
target_include_directories(AureaMatieres PRIVATE ${APP_INCLUDES})
target_link_libraries(AureaMatieres PRIVATE ${APP_LIBRARIES})

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(material_tests tests/test_materials.cpp ${APP_SOURCES})
    target_include_directories(material_tests PRIVATE ${APP_INCLUDES})
    target_link_libraries(material_tests PRIVATE ${APP_LIBRARIES} Qt6::Test)
    add_test(NAME material_module COMMAND material_tests -platform offscreen)
endif()
