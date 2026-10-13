cmake_minimum_required(VERSION 3.16)
project(AureaProduction VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql PrintSupport)
add_executable(AureaProduction WIN32
    main.cpp mainwindow.cpp mainwindow.h pages.cpp pages.h uikit.cpp uikit.h store.cpp store.h
    database.cpp database.h pdfexport.cpp pdfexport.h assets.qrc)
target_link_libraries(AureaProduction PRIVATE Qt6::Widgets Qt6::Sql Qt6::PrintSupport)
