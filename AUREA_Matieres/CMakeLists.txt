cmake_minimum_required(VERSION 3.16)
project(AureaMatieres VERSION 1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql PrintSupport)

add_library(quirc STATIC
    materials/third_party/quirc/quirc.c materials/third_party/quirc/decode.c
    materials/third_party/quirc/identify.c materials/third_party/quirc/version_db.c)
target_include_directories(quirc PUBLIC materials/third_party/quirc)
if(NOT MSVC)
    target_link_libraries(quirc PUBLIC m)
endif()

add_executable(AureaMatieres WIN32
    main.cpp common/icons.h common/appsetup.h
    materials/materialwindow.cpp materials/materialwindow.h materials/materialwindow.ui materials/materials.qrc
    materials/inventoryrepository.cpp materials/inventoryrepository.h
    materials/qrservice.cpp materials/qrservice.h
    materials/reports.cpp materials/reports.h
    materials/third_party/qrcodegen/qrcodegen.cpp
    assets.qrc chevrons.qrc)
target_include_directories(AureaMatieres PRIVATE common materials
    materials/third_party/qrcodegen materials/third_party/quirc)
target_link_libraries(AureaMatieres PRIVATE quirc Qt6::Widgets Qt6::Sql Qt6::PrintSupport)
