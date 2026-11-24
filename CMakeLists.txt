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
    third_party/quirc/quirc.c third_party/quirc/decode.c
    third_party/quirc/identify.c third_party/quirc/version_db.c)
target_include_directories(quirc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/quirc)

set(APP_SOURCES
    Crud/crud_interface.cpp Crud/materialwindow.cpp Crud/materialwindow.h Crud/materialwindow.ui
    Crud/inventoryrepository.cpp Crud/inventoryrepository.h Crud/oracleconnection.cpp Crud/oracleconnection.h
    Metier/qr_interface.cpp Metier/qrservice.cpp Metier/qrservice.h Metier/reapprovisionnement.cpp
    Metier/reports.cpp Metier/reports.h Metier/statistiques_pdf.cpp
    arduino/arduino.cpp arduino/arduino.h arduino/borne_interface.cpp arduino/bornepage.cpp arduino/bornepage.h
    Animations/QrAnimation.cpp Animations/QrAnimation.h Animations/StitchAnimation.cpp Animations/StitchAnimation.h
    Animations/LinkLight.h Animations/Motion.h Animations/UiEffects.h
    Extras/icons.h Extras/sound.h Extras/uihelpers.h
    third_party/qrcodegen/qrcodegen.cpp Resources/materials.qrc)

set(APP_INCLUDES Crud Metier arduino Animations Extras third_party/qrcodegen)
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
